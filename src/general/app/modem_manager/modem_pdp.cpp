#include <algorithm>  // std::min

#include "global_defs.h"
#include "modem_manager.h"
#include "modem_manager_cfg.h"


namespace modem
{

// from modem_manager.cpp
extern MODEM_CLASS mdev;

namespace pdp
{

/*
 * Local Variables
 */
typedef enum
{
    SESSION_REQUEST_NONE = 0,
    SESSION_REQUEST_CREATE,
    SESSION_REQUEST_CLOSE
} session_request_et;

typedef struct
{
    int                     id_session;     // PDP session id (a.k.a socket file descriptor); 0 = vacant
    session_config_st       s_config;       // session config
    int                     n_status;       // +QIOPEN status or negative error code
    unsigned                num_incoming;   // +QIURC "recv" number of bytes to read
    payload_buffer_st       s_send;         // pending data to send
    session_request_et      e_request;      // open or close socker
} protocol_session_st;

static protocol_session_st          as_sessions[MODEM_PDP_MAX_SESSIONS];
static uint8_t                      pdp_buffer[MODEM_UART_RX_FIFO_SIZE] = {0, }; // temp buffer for both rx & tx
static QueueHandle_t                queue_session_requests = NULL;
static SemaphoreHandle_t            mtx_session_access = NULL; // shared access to session list
#define LOCK_SESSIONS_ACCESS()      ((NULL != mtx_session_access) && (pdTRUE == xSemaphoreTake(mtx_session_access, 15000))) /* wait 15sec */
#define UNLOCK_SESSIONS_ACCESS()    ((void)xSemaphoreGive(mtx_session_access))
#define SESSION_CONFIG_DELAY        (3000) // 3sec

// id_session = connectID -  1
#define connectID(id_session)       (id_session - 1)
#define sessionID(id_connect)       (id_connect + 1)
static int                          id_session_count = 0;

/*
 * Private Function Prototypes
 */
static void stopSession(int id_session);
static int searchUdpSession(const session_config_st *ps_config);
static int waitCreatedSession(const session_config_st *ps_config);

static bool processSessionRequests(void);
static bool processSendData(void);
static bool processIncomingData(void);

/*
Exclusive Functions for Modem-Manager only
*/
bool init_sessions(void)
{
    mdev.config_pdp_options();

    if (NULL == mtx_session_access)
    {
        mtx_session_access = xSemaphoreCreateMutex();
        assert(NULL != mtx_session_access);
    }

    if (NULL == queue_session_requests) // initial
    {
        memset(&as_sessions, 0, sizeof(as_sessions));
        // queue of pointers only
        queue_session_requests = xQueueCreate(MODEM_PDP_MAX_SESSIONS, sizeof(protocol_session_st *));
        assert(NULL != queue_session_requests);
    }
    else
    {
        // close all existing sessions, for now
        searchUdpSession(NULL);
    }

    return true;
}

bool process_sessions(void)
{
    bool b_result = true;

    if (false == processSessionRequests())
    {
        b_result = false;
    }
    if (false == processSendData())
    {
        b_result = false;
    }
    if (false == processIncomingData())
    {
        b_result = false;
    }

    return b_result;
}

// call backs functions
void handle_session_status(int id_connect, int n_status)
{
    //LOGD("%s(%d, %d)", __func__, id_connect, n_status);

    for (uint8_t ui8_idx = 0; ui8_idx < MODEM_PDP_MAX_SESSIONS; ui8_idx++)
    {
        protocol_session_st *ps_session = &as_sessions[ui8_idx];
        if (sessionID(id_connect) == ps_session->id_session)
        {
            ps_session->n_status = n_status;
            break; // matched
        }
    }
}

void handle_incoming_data(int id_connect, unsigned num_bytes)
{
    //LOGD("%s(%d, %u)", __func__, id_connect, num_bytes);

    for (uint8_t ui8_idx = 0; ui8_idx < MODEM_PDP_MAX_SESSIONS; ui8_idx++)
    {
        protocol_session_st *ps_session = &as_sessions[ui8_idx];
        if (sessionID(id_connect) == ps_session->id_session)
        {
            ps_session->num_incoming = num_bytes;
            break; // matched
        }
    }
}

/*
 * Public Functionss
 */

// returns id of created session
int request_session(const session_config_st *ps_config)
{
    protocol_session_st *ps_session = NULL;
    uint8_t ui8_idx;
    int id_session;

    // udp sockets only, for now
    assert( CellularModem::PDP_PROTOCOL_UDP == ps_config->e_protocol);

    id_session = -1;
    if (NULL == queue_session_requests)
    {
        // not yet initialized
    }
    else if (LOCK_SESSIONS_ACCESS())
    {
        for (ui8_idx = 0; ui8_idx < MODEM_PDP_MAX_SESSIONS; ui8_idx++)
        {
            ps_session = &as_sessions[ui8_idx];
            if ((ps_session->id_session > 0) &&
                (0 == memcmp(&ps_session->s_config, ps_config, sizeof(ps_session->s_config))))
            {
                break; // already in the list
            }
            if (0 == ps_session->id_session) // if vacant
            {
                // request a new one
                memcpy(&ps_session->s_config, ps_config, sizeof(ps_session->s_config));
                ps_session->id_session = ++id_session_count;
                if (ps_session->id_session > MODEM_PDP_MAX_SESSIONS) {
                    ps_session->id_session = 1;
                    id_session_count = 1;
                }
                ps_session->e_request = SESSION_REQUEST_CREATE;
                //LOGD("udp: request session id %d: %s:%u",
                //    ps_session->id_session, ps_session->s_config.ac_address, ps_session->s_config.u_port);
                if (pdPASS != xQueueSend(queue_session_requests, &ps_session, 10000)) {
                    LOGW("udp: request failed");
                    ps_session = NULL;
                }
                break;
            }
            ps_session = NULL;
        }
        UNLOCK_SESSIONS_ACCESS();
    }

    if (NULL != ps_session)
    {
        id_session = waitCreatedSession(ps_config);
    }

    return id_session;
}

bool close_session(int id_session)
{
    protocol_session_st *ps_session;
    uint8_t ui8_idx;
    bool b_result = false;
    if (LOCK_SESSIONS_ACCESS())
    {
        for (ui8_idx = 0; ui8_idx < MODEM_PDP_MAX_SESSIONS; ui8_idx++)
        {
            ps_session = &as_sessions[ui8_idx];
            if (ps_session->id_session == id_session) // existing in the local list
            {
                ps_session->e_request = SESSION_REQUEST_CLOSE;
                b_result = (pdPASS == xQueueSend(queue_session_requests, &ps_session, 3000));
                break;
            }
        }
        UNLOCK_SESSIONS_ACCESS();
    }

    return b_result;
}

bool send_data(int id_session, const uint8_t *pui8_data, uint16_t ui16_length)
{
    protocol_session_st *ps_session;
    uint8_t ui8_idx;
    bool b_result;

    //LOGD("%s: %d-%u %.*s", __func__, id_session, ui16_length, ui16_length, pui8_data);

    b_result = false;
    if (LOCK_SESSIONS_ACCESS())
    {
        for (ui8_idx = 0; ui8_idx < MODEM_PDP_MAX_SESSIONS; ui8_idx++)
        {
            ps_session = &as_sessions[ui8_idx];
            if (ps_session->id_session == id_session)
            {
                // check if empty buffer && socket is ready
                //if ((0 == ps_session->s_send.sz_length) && (1 == ps_session->n_status))
                if (0 == ps_session->s_send.sz_length)
                {
                    ui16_length = std::min(ui16_length, (uint16_t)sizeof(ps_session->s_send.aui8_buff));
                    memcpy(ps_session->s_send.aui8_buff, pui8_data, ui16_length);
                    ps_session->s_send.sz_length    = ui16_length;
                    b_result = true;
                }
                break; // matched
            }
        }
        UNLOCK_SESSIONS_ACCESS();
    }

    return b_result;
}

int get_error(void)
{
    return mdev.last_pdp_error();
}


/*
 * Private Function Prototypes
 */

static void stopSession(int id_session)
{
    if (id_session > 0)
    {
        (void)sendAT(&mdev, K_MODEM_STR_CMD_PDP_CLOSE_SOCKET "=%d", connectID(id_session));
        (void)mdev.waitResponseTimeout(10*1000UL, "OK");
    }
}

static int searchUdpSession(const session_config_st *ps_config)
{
    session_config_st s_cfg;
    char        ac_service[16]; // service type
    int         id_connect;     // connectID (socket fd index)
    int         id_ctx;         // contextID
    int         n_port;         // local port = 0 (automatic)
    int         n_state;        // bg9x_pdp_state_et
    int         num_info; // number of parsed parameters
    uint16_t    ui16_bytes_read;
    char       *pc_info = NULL; // split info lines
    char       *pc_save = NULL; // for strtok_r

    memset(pdp_buffer, 0, sizeof(pdp_buffer));
    if (!sendAT(&mdev, K_MODEM_STR_CMD_QUERY_SOCKET_STATUS "?"))
    {
        //
    }
    else if ((ui16_bytes_read = mdev.read(pdp_buffer, sizeof(pdp_buffer) - 1,
                                            "OK\r\n", MODEM_COMMS_RX_LINE_TIMEOUT, 3000UL)) > 0)
    {
        //LOGD("sockets: %s", pdp_buffer);
        pc_info = strtok_r((char *)pdp_buffer, "\r\n", &pc_save);
        while (NULL != pc_info)
        {
            num_info = sscanf(pc_info, K_MODEM_STR_CMD_QUERY_SOCKET_STATUS ": %d,\"%15[^\"]\",\"%31[^\"]\",%u,%d,%d,%d",
                              &id_connect, ac_service, s_cfg.ac_address, &s_cfg.u_port, &n_port, &n_state, &id_ctx);
            //LOGD("socket info (%d) %s", num_info, pc_info);
            if (7 == num_info) // if parsed successfully
            {
                if (NULL == ps_config) // close all sessions if no specified info
                {
                    stopSession(sessionID(id_connect));
                }
              #if 0
                else if ((0 == strncmp(ps_config->ac_address, s_cfg.ac_address, sizeof(ps_config->ac_address))) &&
                         (ps_config->u_port == s_cfg.u_port))
              #else // domain names are resolve to ip address
                else if (ps_config->u_port == s_cfg.u_port)
              #endif
                {
                    LOGI("udp session %d %s:%u", sessionID(id_connect), s_cfg.ac_address, s_cfg.u_port);
                    return sessionID(id_connect);
                }
            }
            pc_info = strtok_r(NULL, "\r\n", &pc_save);
        }
    }

    return -1;
}

static int waitCreatedSession(const session_config_st *ps_config)
{
    protocol_session_st *ps_session;
    int id_session;
    int n_retry = 0;
    uint8_t ui8_idx;

    id_session = -1;
    do {
        // if request was processed by modem-manager ...
        if (pdFALSE == uxQueueMessagesWaiting(queue_session_requests))
        {
            if (LOCK_SESSIONS_ACCESS())
            {
                for (ui8_idx = 0; ui8_idx < MODEM_PDP_MAX_SESSIONS; ui8_idx++)
                {
                    ps_session = &as_sessions[ui8_idx];
                  #if 0
                    if ((0 == strncmp(ps_config->ac_address, ps_session->s_config.ac_address, sizeof(ps_config->ac_address))) &&
                        (ps_config->u_port == ps_session->s_config.u_port))
                  #else // domain names are resolve to ip address
                    if (ps_config->u_port == ps_session->s_config.u_port)
                  #endif
                    {
                        id_session = ps_session->id_session;
                        break; // matched
                    }
                }
                UNLOCK_SESSIONS_ACCESS();
            }
            break;
        }
        delayms(100);
    } while (++n_retry < 200);

    return id_session;
}

static bool processSessionRequests(void)
{
    protocol_session_st *ps_session = NULL;
    int     id_session;
    bool    b_status = true;


    if ((pdPASS == (xQueuePeek(queue_session_requests, &ps_session, 0))) && (NULL != ps_session))
    {
        //LOGD("%s: proto %d", __func__, ps_session->s_config.e_protocol);

        (void)LOCK_SESSIONS_ACCESS();

        switch (ps_session->e_request)
        {
        case SESSION_REQUEST_CREATE:
            // check if already existing
            if ((id_session = searchUdpSession(&ps_session->s_config)) > 0)
            {
                //LOGD("udp: session %d %s:%u", id_session,
                //     ps_session->s_config.ac_address, ps_session->s_config.u_port);
                ps_session->id_session = id_session;
                // remove from queue
                ps_session->e_request  = SESSION_REQUEST_NONE;
                (void)xQueueReceive(queue_session_requests, &ps_session, 0);
            }
            //  try create new socket
            else if (!sendAT(&mdev, K_MODEM_STR_CMD_PDP_OPEN_SOCKET "=%d,%d,\"%s\",\"%s\",%u",
                            mdev.s_cnx_cfg.id_ctx, connectID(ps_session->id_session), "UDP" /*UDP client*/,
                            ps_session->s_config.ac_address, ps_session->s_config.u_port /*host address and port*/))
            {
                //
            }
            else if (!mdev.waitResponseTimeout(15 *1000UL, "OK")) // fix me: max response time is 2.5minutes
            {
                LOGW("udp: failed config \"%s\" (error %d)", ps_session->s_config.ac_address, mdev.get_last_error());
                b_status = false;
                delayms(SESSION_CONFIG_DELAY);
                if (mdev.last_pdp_error() > 0)
                {
                    // close all sessions/sockets
                    searchUdpSession(NULL);
                    memset(&as_sessions, 0, sizeof(as_sessions));
                    while (pdTRUE == xQueueReceive(queue_session_requests, &ps_session, 0)) { }
                    // will request new data connection
                    mdev.deactivate_pdp((CellularModem::pdp_ctx_et)mdev.s_cnx_cfg.id_ctx); // force reactivate
                    LOGW("udp: deactivate pdp", NULL);
                }
            }
            // verify newly configured session
            else if ((id_session = searchUdpSession(&ps_session->s_config)) < 1)
            {
                LOGW("udp: session not configured");
            }
            else
            {
                // success creating session
                ps_session->id_session  = id_session;
                delayms(SESSION_CONFIG_DELAY); // wait for session to be ready (prevent cme error 923)
                LOGI("udp: new session %d %s:%u", id_session,
                     ps_session->s_config.ac_address, ps_session->s_config.u_port);
                // remove from queue
                ps_session->e_request   = SESSION_REQUEST_NONE;
                (void)xQueueReceive(queue_session_requests, &ps_session, 0);
            }
            break;

        default: // just close it, ignore error ...
            stopSession(ps_session->id_session);
            // clear request
            memset(ps_session, 0, sizeof(protocol_session_st));
            // remove from queue
            (void)xQueueReceive(queue_session_requests, &ps_session, 0);
            break;
        }

        UNLOCK_SESSIONS_ACCESS();
    }

    return b_status;
}

static bool processSendData(void)
{
    protocol_session_st *ps_session;
    uint16_t ui16_len;
    uint8_t ui8_idx;
    bool b_result;

    b_result = true;
    if (false == manager::has_data_connection())
    {
        //LOGW("no data connection yet");
    }
    else if (LOCK_SESSIONS_ACCESS())
    {
        for (ui8_idx = 0; ui8_idx < MODEM_PDP_MAX_SESSIONS; ui8_idx++)
        {
            ps_session = &as_sessions[ui8_idx];
            // check if has pending data to send ...
            if (0 != ps_session->s_send.sz_length)
            {
                //LOGD("%s: %d-%u %.*s", __func__,
                //      ps_session->id_session, ps_session->s_send.sz_length,
                //      ps_session->s_send.sz_length, ps_session->s_send.aui8_buff);

                ui16_len = std::min(ps_session->s_send.sz_length, (size_t) sizeof(pdp_buffer) -1);
                memcpy(pdp_buffer, ps_session->s_send.aui8_buff, ui16_len);

                b_result = false;
                if (!sendAT(&mdev, K_MODEM_STR_CMD_PDP_SEND_DATA "=%d,%u", connectID(ps_session->id_session), ui16_len))
                {
                    // LOGW("udp send failed");
                }
                else if (mdev.waitResponseTimeout(1000, K_MODEM_STR_PDP_READY_TO_SEND) < 1)
                {
                    LOGW("no 'ready to send' prompt");
                }
                else if (ui16_len != mdev.write(pdp_buffer, ui16_len))
                {
                    LOGW("write data failed");
                }
                else if (mdev.waitResponseTimeout(3000, K_MODEM_STR_PDP_RESP_SEND_OK) < 1)
                {
                    LOGW("udp send failed");
                }
                else
                {
                    //LOGD("sent %u bytes", ui16_len);
                    //LOGB(pdp_buffer, ui16_len);
                    ps_session->s_send.sz_length = 0;
                    b_result = true;
                }
                break;
            }
        }
        UNLOCK_SESSIONS_ACCESS();
    }

    return b_result;
}

static bool processIncomingData(void)
{
    protocol_session_st *ps_session;
    uint8_t *pui8_data;
    uint16_t ui16_bytes_read;


    if (LOCK_SESSIONS_ACCESS())
    {
        for (uint8_t ui8_idx = 0; ui8_idx < MODEM_PDP_MAX_SESSIONS; ui8_idx++)
        {
            ps_session = &as_sessions[ui8_idx];
            // check if has pending data to receive ...
            if (0 != ps_session->num_incoming)
            {
                //LOGD("%s: %d %u", __func__, ps_session->id_session, ps_session->num_incoming);

                while ((NULL != (pui8_data = pdp_buffer)) &&
                       (ui16_bytes_read = mdev.retrieve_pdp_data(connectID(ps_session->id_session), &pui8_data, sizeof(pdp_buffer))) > 0)
                {
                    // execute callback function
                    //assert(NULL != ps_session->s_config.fpv_receive_cb);
                    ps_session->s_config.fpv_receive_cb(pui8_data, ui16_bytes_read);
                }

                ps_session->num_incoming = 0;
                break;
            }
        }
        UNLOCK_SESSIONS_ACCESS();
    }

    return true;
}


} // namespace modem::pdp

} // namespace modem
