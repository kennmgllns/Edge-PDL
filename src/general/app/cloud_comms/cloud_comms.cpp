#include <algorithm>  // std::min
#include "global_defs.h"
#include "modem_manager.h"
#include "cloud_comms.h"


namespace mngr = modem::manager;
namespace pdp = modem::pdp;

namespace cloud::comms
{

/*
 * Local Constants
 */
#define K_CLOUD_COMMS_SEND_QUEUE_SIZE       (8)

/*
 * Local Variables
 */
static state_et             e_state;        // cloud comms state

static struct {
    udp_state_et            e_state;
    int                     id_session;     // UDP socket fd
    pdp::session_config_st  s_config;       // remote host configuration
    QueueHandle_t           queue_send;     // queue of payload data to send (contains pointer to elements of 'as_send_queue')
    pdp::payload_buffer_st  as_send_queue[K_CLOUD_COMMS_SEND_QUEUE_SIZE];  // reserved area for data pending to be send
    SemaphoreHandle_t       mtx_receive;    // mutex shared access to the rx buffer
    pdp::payload_buffer_st  s_receive_buff; // buffer for modem manager callbacks
    struct {
        bool                b_state;        // true = connected (i.e. got a udp socket)
        uint8_t             ui8_retry;      // retry count
        uint32_t            ms_start;       // millisecond timestamp of udp session request
        uint32_t            ms_last_send;   // millisecond timestamp of last attempt to send (attempt to queue data to modem-manager)
    } s_conn_status; // connection status
} s_udp_ctx; // UDP connection context


static struct {
    dtls_state_et           e_state;
    dtls_client_context_st  s_client;       // DTLS client
    struct {
        bool                b_state;        // true = connected (i.e. handshake done)
        bool                b_initial_ok;   // true = got a successful initial connection
        uint8_t             ui8_retry;      // retry count
        uint32_t            ms_start;       // millisecond timestamp of handshake request
    } s_conn_status; // connection status
} s_dtls_ctx; // DTLS connection context


/*
 * Private Function Prototypes
 */
static bool processUdpState(void);
static bool handleUdpReceive(const uint8_t *pui8_data, uint16_t ui16_length);

static bool processDtlsState(void);
static bool cloudNetSend(const uint8_t *pui8_buff, size_t sz_len);
static int dtlsSendHandler(uint8_t *pui8_buf, size_t sz_buf_len);
static int dtlsRespHandler(uint8_t *pui8_buf, size_t sz_buf_len);
static int dtlsPskInfo(dtls_credentials_type_et type, const uint8_t *desc, size_t desc_len, uint8_t *result, size_t result_length);
static int dtlsEventHandler(dtls_alert_level_et level, unsigned short code);

/*
 * Public Functions
 */
bool init()
{
    LOGD("core %u: %s", xPortGetCoreID(), __PRETTY_FUNCTION__);

    memset(&s_udp_ctx, 0, sizeof(s_udp_ctx));
    s_udp_ctx.s_config.e_protocol       = CellularModem::PDP_PROTOCOL_UDP;
    s_udp_ctx.s_config.fpv_receive_cb   = handleUdpReceive;

    s_udp_ctx.queue_send = xQueueCreate(K_CLOUD_COMMS_SEND_QUEUE_SIZE, sizeof(pdp::payload_buffer_st *));
    assert(NULL != s_udp_ctx.queue_send);

    s_udp_ctx.mtx_receive = xSemaphoreCreateMutex();
    assert(NULL != s_udp_ctx.mtx_receive);
    s_udp_ctx.e_state = UDP_STATE_INIT;

    memset(&s_dtls_ctx, 0, sizeof(s_dtls_ctx));
    s_dtls_ctx.s_client.s_handler.write         = dtlsSendHandler;
    s_dtls_ctx.s_client.s_handler.read          = dtlsRespHandler;
    s_dtls_ctx.s_client.s_handler.get_psk_info  = dtlsPskInfo;
    s_dtls_ctx.s_client.s_handler.event         = dtlsEventHandler;
    s_dtls_ctx.e_state                          = DTLS_STATE_INIT;

    net::init(cloudNetSend);
    e_state  = STATE_INIT;
    return true;
}

void cycle()
{
    switch (e_state)
    {
    case STATE_INIT:
        e_state = STATE_DTLS_SESSION;
        break;

    case STATE_DTLS_SESSION:
        processUdpState();
        processDtlsState();
        if (true == s_dtls_ctx.s_conn_status.b_state)
        {
            e_state = STATE_SYNC_TIME;
        }
        break;

    case STATE_SYNC_TIME: // fall-through
    case STATE_SEND_REPORTS:
    case STATE_SERVER_REQUESTS:
        sync::cycle();
        if (true == sync::getStatus())
        {
            status::cycle();
            monitor::cycle();
            event::cycle();
            update::cycle();
            commands::cycle();
        }
        e_state = STATE_DTLS_SESSION;
        break;

    case STATE_IDLE:
        break;

    default:
        e_state = STATE_INIT;
        break;
    }
}

// [re]connect
void connect(void)
{
    disconnect();
    flush_buff();

    // will auto re-connect ...
}

// close dtls & udp session
void disconnect(void)
{
    setCommsFlag(DTLS_COMMS, false);
    setCommsFlag(CLOUD_CONN, false);

    // disconnect dtls
    if (true == s_dtls_ctx.s_conn_status.b_state)
    {
        (void)dtls_close(&s_dtls_ctx.s_client);
        s_dtls_ctx.s_conn_status.b_state = false;
    }
    s_dtls_ctx.e_state = DTLS_STATE_INIT;

    // disconnect udp
    if (s_udp_ctx.id_session > 0)
    {
        (void)pdp::close_session(s_udp_ctx.id_session);
    }
    s_udp_ctx.e_state = UDP_STATE_INIT;

}

// cancel all pending UDP data to send
void flush_buff(void)
{
    //LOGW("%s", __func__);
    BaseType_t result;
    pdp::payload_buffer_st *ps_payload = NULL;
    do { // remove queues
        result = xQueueReceive(s_udp_ctx.queue_send, & ps_payload, 10);
    } while (pdTRUE == result);
    // clear buffer
    memset(s_udp_ctx.as_send_queue, 0, sizeof(s_udp_ctx.as_send_queue));
}

/*
 * Private Functions
 */
static bool processUdpState(void)
{
    pdp::payload_buffer_st  *ps_payload;
    const char              *pc_apn = NULL;
    bool                     b_cnx_state;

    switch (s_udp_ctx.e_state)
    {
    case UDP_STATE_INIT:
        s_udp_ctx.id_session              = -1; // invalid id
        s_udp_ctx.s_conn_status.ui8_retry = 0;
        s_udp_ctx.s_conn_status.ms_last_send = 0;
        s_udp_ctx.s_conn_status.b_state   = false;
        s_udp_ctx.e_state                 = UDP_STATE_CONNECT;
        break;

    case UDP_STATE_CONNECT:
        if ((false == mngr::get_connection_state(&b_cnx_state, NULL, &pc_apn)) ||
            (false == b_cnx_state) || (NULL == pc_apn) || ('\0' == *pc_apn))
        {
            //LOGD("not yet connected, state=%d apn=\"%s\"", b_cnx_state, pc_apn);
        }
        else
        {
            if (0 == strncmp(pc_apn, K_CLOUD_PRIVATE_APNAME, __builtin_strlen(K_CLOUD_PRIVATE_APNAME)))
            {
                strncpy(s_udp_ctx.s_config.ac_address, K_CLOUD_PRIVATE_HOST_ADDR, sizeof(s_udp_ctx.s_config.ac_address) - 1);
            }
            else
            {
                strncpy(s_udp_ctx.s_config.ac_address, K_CLOUD_HOST_ADDR, sizeof(s_udp_ctx.s_config.ac_address) - 1);
            }
            s_udp_ctx.s_config.u_port        = K_CLOUD_HOST_PORT;
            s_udp_ctx.s_conn_status.ms_start = millis();
            s_udp_ctx.e_state                = UDP_STATE_CONNECTING;
            //LOGD("remote %s:%u", s_udp_ctx.s_config.ac_address, s_udp_ctx.s_config.u_port);
        }
        break;

    case UDP_STATE_CONNECTING:
        if ((s_udp_ctx.id_session = pdp::request_session(&s_udp_ctx.s_config)) > 0)
        {
            //LOGD("udp socket = %d", s_udp_ctx.id_session);
            s_udp_ctx.s_conn_status.b_state   = true;
            s_udp_ctx.s_conn_status.ui8_retry = 0;
            s_udp_ctx.e_state                 = UDP_STATE_READY;
        }
        /*
        else if (millis() - s_udp_ctx.s_conn_status.ms_start > (10*1000))
        {
            // request timeout
        }
        */
        else
        {
            //LOGD("no udp session yet");
            s_udp_ctx.e_state  = UDP_STATE_CLOSE;
        }
        break;

    case UDP_STATE_CLOSE:
        if (s_udp_ctx.id_session > 0)
        {
            (void)pdp::close_session(s_udp_ctx.id_session);
        }

        if (++s_udp_ctx.s_conn_status.ui8_retry < 3)
        {
            s_udp_ctx.e_state = UDP_STATE_CONNECT;
        }
        else
        {
            //LOGW("udp: max retry reached");
            // to do: tell modem-manager to get a new ip address
            s_udp_ctx.e_state = UDP_STATE_INIT;
        }
        break;

    case UDP_STATE_READY:
        // check if has something to send
        if (pdTRUE == xQueuePeek(s_udp_ctx.queue_send, &ps_payload, 0))
        {
            //LOGD("got pending %lu bytes", ps_payload->sz_length);
            if (true == pdp::send_data(s_udp_ctx.id_session, ps_payload->aui8_buff, ps_payload->sz_length))
            {
                ps_payload->sz_length = 0; // 0 = already consumed
                (void)xQueueReceive(s_udp_ctx.queue_send, &ps_payload, 0); // remove from list
                s_udp_ctx.s_conn_status.ms_last_send = millis();
            }
            else
            {
                //LOGW("udp: forward payload failed");
                if ((pdp::get_error() > 0) &&
                    (0 != s_udp_ctx.s_conn_status.ms_last_send) &&
                    (millis() - s_udp_ctx.s_conn_status.ms_last_send > MODEM_PDP_ADDR_CHECK_INTERVAL))
                {
                    connect(); // force re-fresh session (back to init state)
                }
                else
                {
                    s_udp_ctx.e_state = UDP_STATE_CONNECT; // re-check if session is still valid
                }
            }
            s_udp_ctx.s_conn_status.ms_last_send = millis();
        }
        // check if has something to process received data
        if (pdTRUE == xSemaphoreTake(s_udp_ctx.mtx_receive, 0))
        {
            // if has receive data ...
            if (0 != s_udp_ctx.s_receive_buff.sz_length)
            {
                if (0 != dtls_handle_message(&s_dtls_ctx.s_client,
                                             s_udp_ctx.s_receive_buff.aui8_buff,
                                             s_udp_ctx.s_receive_buff.sz_length))
                {
                    //LOGW("dtls: message not handled ?");
                }
                else
                {
                    //LOGI("rcv [%u]: %.*s", s_udp_ctx.s_receive_buff.sz_length,
                    //      s_udp_ctx.s_receive_buff.sz_length, s_udp_ctx.s_receive_buff.aui8_buff);
                }
                s_udp_ctx.s_receive_buff.sz_length = 0; // set to zero to indicate that it is already proccessed
            }
            (void)xSemaphoreGive(s_udp_ctx.mtx_receive);
        }
        break;

    case UDP_STATE_IDLE:
        break;

    default:
        s_udp_ctx.e_state = UDP_STATE_INIT;
        break;
    }

    return true;
}

static bool handleUdpReceive(const uint8_t *pui8_data, uint16_t ui16_length)
{
    int n_retry = 0;
    size_t sz_len;
    bool b_result = false;

    //LOGD("rcv [%u]: %.*s", ui16_length, ui16_length, pui8_data);

    do {
        if (pdTRUE == xSemaphoreTake(s_udp_ctx.mtx_receive, 0))
        {
            // check if previous rx was alaready processed
            if (0 == s_udp_ctx.s_receive_buff.sz_length)
            {
                sz_len = std::min(ui16_length, (uint16_t) sizeof(s_udp_ctx.s_receive_buff.aui8_buff));
                memcpy(s_udp_ctx.s_receive_buff.aui8_buff, pui8_data, sz_len);
                s_udp_ctx.s_receive_buff.sz_length = sz_len;
                b_result = true;
            }
            (void)xSemaphoreGive(s_udp_ctx.mtx_receive);
        }
        delayms(5);
    } while ((false == b_result) && (++n_retry < 100));

    if (false == b_result)
    {
        LOGW("udp rx not handled [%u]: %.*s", ui16_length, ui16_length, pui8_data);
    }

    return b_result;
}

static bool processDtlsState(void)
{
    bool b_result = true;

    switch (s_dtls_ctx.e_state)
    {
    case DTLS_STATE_INIT:
        dtls_client_init(&s_dtls_ctx.s_client);
        s_dtls_ctx.s_conn_status.ui8_retry = 0;
        s_dtls_ctx.s_conn_status.b_state   = false;
        s_dtls_ctx.e_state                 = DTLS_STATE_CONNECT;
        break;

    case DTLS_STATE_CONNECT:
        if (UDP_STATE_READY != s_udp_ctx.e_state)
        {
            // no udp link yet
        }
        else if (false == dtls_connect(&s_dtls_ctx.s_client))
        {
            LOGW("dtls: connect error");
            connect(); // restart session
            b_result = false;
        }
        else
        {
            LOGD("dtls: connect start");
            s_dtls_ctx.s_conn_status.b_state  = false;
            s_dtls_ctx.s_conn_status.ms_start = millis();
            s_dtls_ctx.e_state = DTLS_STATE_CONNECTING;
        }
        break;

    case DTLS_STATE_CONNECTING:
        if (true == s_dtls_ctx.s_conn_status.b_state)
        {
            LOGI("dtls: connect ok");
            s_dtls_ctx.s_conn_status.ui8_retry    = 0;
            s_dtls_ctx.s_conn_status.b_initial_ok = true;
            s_dtls_ctx.e_state                    = DTLS_STATE_READY;
            setCommsFlag(DTLS_COMMS, true);
        }
        else if (millis() - s_dtls_ctx.s_conn_status.ms_start > K_CLOUD_DTLS_CONN_TIMEOUT)
        {
            LOGW("dtls: connect timeout");
            s_dtls_ctx.e_state = DTLS_STATE_CLOSE;
        }
        break;

    case DTLS_STATE_CLOSE:
        setCommsFlag(DTLS_COMMS, false);
        if (0 != dtls_close(&s_dtls_ctx.s_client))
        {
            LOGW("dtls: close error");
        }
        else
        {
            //LOGD("dtls: close ok");
        }

        // restart udp session if has no previous dtls connection or max retry reached
        if ((false == s_dtls_ctx.s_conn_status.b_initial_ok) ||
            (++s_dtls_ctx.s_conn_status.ui8_retry >= K_CLOUD_DTLS_CONN_RETRY_LIM))
        {
            if (s_dtls_ctx.s_conn_status.ui8_retry >= K_CLOUD_DTLS_CONN_RETRY_LIM) {
                LOGW("dtls: max retry reached");
            }
            s_udp_ctx.e_state  = UDP_STATE_CLOSE; // restart udp session
            s_dtls_ctx.e_state = DTLS_STATE_INIT;
            b_result = false;
        }
        else
        {
            // retry connection
            s_dtls_ctx.e_state = DTLS_STATE_CONNECT;
        }
        break;

    case DTLS_STATE_READY:
        if (UDP_STATE_READY != s_udp_ctx.e_state)
        {
            // wait for udp to reestablish, then retry connection
            //s_dtls_ctx.e_state = CLOUD_DTLS_STATE_CONNECT;
        }
        break;

    case DTLS_STATE_IDLE:
        break;

    default:
        s_dtls_ctx.e_state = DTLS_STATE_INIT;
        break;
    }

    return b_result;
}

static bool cloudNetSend(const uint8_t *pui8_buff, size_t sz_len)
{
    bool b_result;
    b_result = dtls_write(&s_dtls_ctx.s_client, pui8_buff, sz_len);
    //LOGD("%s: dtls_write(%lu) = %d", __func__, sz_len, b_result);
    return b_result;
}

static int dtlsSendHandler(uint8_t *pui8_buf, size_t sz_buf_len)
{
    pdp::payload_buffer_st *ps_payload = NULL;
    size_t sz_len;
    uint8_t ui8_idx;

    //LOGW("%s(%lu)", __func__, sz_buf_len);

    sz_len = 0;
    for (ui8_idx = 0; ui8_idx < K_CLOUD_COMMS_SEND_QUEUE_SIZE; ui8_idx++)
    {
        if (0 == s_udp_ctx.as_send_queue[ui8_idx].sz_length)
        {
            ps_payload = &s_udp_ctx.as_send_queue[ui8_idx];
            break;
        }
    }
    if (NULL != ps_payload)
    {
        sz_len = std::min(sz_buf_len, (size_t)sizeof(ps_payload->aui8_buff));
        memcpy(ps_payload->aui8_buff, pui8_buf, sz_len);
        ps_payload->sz_length = sz_len;
        if (pdTRUE == xQueueSend(s_udp_ctx.queue_send, &ps_payload, 3000))
        {
            //LOGD("udp queue %lu bytes", sz_len);
        }
        else
        {
            LOGW("udp queue failed");
            ps_payload->sz_length = 0;
            sz_len = 0;
        }
    }

    return (int)sz_len;
}

static int dtlsRespHandler(uint8_t *pui8_buf, size_t sz_buf_len)
{
    //LOGD("%s(%p, %lu)", __func__, pui8_buf, sz_buf_len);
    if (false == net::handleMessage(pui8_buf, sz_buf_len))
    {
        LOGW("coap: parse failed");
    }
    return 0;
}

static int dtlsPskInfo(dtls_credentials_type_et type, const uint8_t *desc, size_t desc_len, uint8_t *result, size_t result_length)
{
    static size_t psk_id_length  = __builtin_strlen(K_CLOUD_DTLS_PSK_ID);
    static size_t psk_key_length = __builtin_strlen(K_CLOUD_DTLS_PSK_KEY);

    ///LOGD("%s %d", __func__, type);

    switch (type)
    {
    case DTLS_PSK_IDENTITY:
        assert(result_length >= psk_id_length);
        memcpy(result, K_CLOUD_DTLS_PSK_ID, psk_id_length);
        return psk_id_length;
    case DTLS_PSK_KEY:
        assert(result_length >= psk_key_length);
        memcpy(result, K_CLOUD_DTLS_PSK_KEY, psk_key_length);
        return psk_key_length;
    case DTLS_PSK_HINT:
    default:
        LOGW("unsupported request type: %d", type);
    }

    return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
}

static int dtlsEventHandler(dtls_alert_level_et level, unsigned short code)
{
    //LOGD("event level %d code %X", level, code);

    if (level >= DTLS_ALERT_LEVEL_WARNING)
    {
        s_dtls_ctx.e_state = DTLS_STATE_INIT;
    }
    else if (DTLS_EVENT_CONNECTED == code)
    {
        s_dtls_ctx.s_conn_status.b_state = true;
    }
    return 0;
}

} // namespace cloud::comms
