
#include "global_defs.h"
#include "edge_payload/edge_payload.h"
#include "coap/coap_client.h"
#include "cloud_comms.h"


namespace cloud::net
{

/*
 * Local Variables
 */
static struct {
    coap_client_context_st  s_client;           // coap-client instance
    send_func_pt            fpb_send_handler;   // call back for sending dtls payload
    uint16_t                ui16_message_id_ctr;// incremetal message id
    struct {
        uint32_t            ui32_sent_ctr;      // sent counter
        uint32_t            ui32_recv_ctr;      // receive counter
        uint32_t            ui32_timeout_ctr;   // timeout counter
        uint32_t            ui32_reconnect_ctr; // reconnect retry counter
    } s_stats; // comms stats
} s_coap_ctx; // CoAP connection context

/*
 * Private Function Prototypes
 */
static int coapSendHandler(const uint8_t *pui8_buf, uint16_t ui16_len);
static void coapRespHandler(coap_packet_st *ps_resp_packet);


/*
 * Public Functions
 */
void init(send_func_pt fpb_send_handler)
{
    memset(&s_coap_ctx, 0, sizeof(s_coap_ctx));
    s_coap_ctx.s_client.fpi_send_handler = coapSendHandler;
    s_coap_ctx.s_client.fpv_resp_handler = coapRespHandler;

    s_coap_ctx.fpb_send_handler          = fpb_send_handler;

    sync::init();
    status::init();
    monitor::init();
    event::init();
    update::init();
    commands::init();
}

uint16_t newMessageId(void)
{
    uint16_t id_msg;
    id_msg = ++s_coap_ctx.ui16_message_id_ctr;
    return id_msg;
}

bool sendGetRequest(uint16_t ui16_msg_id, const char *pc_path, const uint8_t *pui8_payload, size_t sz_payload_len)
{
    return coapClientSendGetRequest(&s_coap_ctx.s_client, ui16_msg_id, pc_path,
                                    pui8_payload, sz_payload_len);
}

bool sendPutRequest(uint16_t ui16_msg_id, const char *pc_path, const uint8_t *pui8_payload, size_t sz_payload_len)
{
    return coapClientSendPutRequest(&s_coap_ctx.s_client, ui16_msg_id, pc_path,
                                    pui8_payload, sz_payload_len);
}

bool handleMessage(const uint8_t *pui8_buf, size_t sz_buf_len)
{
    return coapClientHandleMsg(&s_coap_ctx.s_client, pui8_buf, sz_buf_len);
}


void timeoutOccured(void)
{
    s_coap_ctx.s_stats.ui32_timeout_ctr++;
    if (++s_coap_ctx.s_stats.ui32_reconnect_ctr >= K_CLOUD_RESPONSE_TIMEOUT_RETRY_LIM)
    {
        LOGW("timeout: connect retry");
        // reconnect to server
        comms::connect();
        s_coap_ctx.s_stats.ui32_reconnect_ctr = 0;
        setCommsFlag(CLOUD_CONN, false);
    }
}

/*
 * Private Functions
 */
static int coapSendHandler(const uint8_t *pui8_buf, uint16_t ui16_len)
{
    if ((NULL != s_coap_ctx.fpb_send_handler) &&
        (true  == s_coap_ctx.fpb_send_handler(pui8_buf, ui16_len)))
    {
        s_coap_ctx.s_stats.ui32_sent_ctr++;
        return (int)ui16_len;
    }
    return -1; // error
}

static void coapRespHandler(coap_packet_st *ps_resp_packet)
{
    const uint8_t  *pui8_recv_ptr;
    size_t          sz_recv_len;
    uint16_t        ui16_message_id;
    uint8_t         ui8_resp_byte;

    ui16_message_id = ((uint16_t)ps_resp_packet->s_header.ui8_msg_id_h << 8) | ps_resp_packet->s_header.ui8_msg_id_l;

    //LOGD("coap: received id=%u len=%u", ui16_message_id, ps_resp_packet->ui16_payloadlen);
    s_coap_ctx.s_stats.ui32_recv_ctr++;
    s_coap_ctx.s_stats.ui32_reconnect_ctr = 0; // reset timeout-reconnect counter if got any response

    if (COAP_TYPE_ACKNOWLEDGEMENT != ps_resp_packet->s_header.ui2_type)
    {   // server-side error (e.g. server was unable to parse the payload)
        LOGW("not ack (resp type=%u)", ps_resp_packet->s_header.ui2_type);
        ui8_resp_byte = EP_SERVER_RESP_INVALID_DATA_ERROR; // assume payload sent was malformed
        pui8_recv_ptr = &ui8_resp_byte;
        sz_recv_len   = 1;
    }
    else
    {   // success: request was acknowledged
        pui8_recv_ptr = ps_resp_packet->pui8_payload;
        sz_recv_len   = ps_resp_packet->ui16_payloadlen;
    }

    sync::handleResponse(ui16_message_id, pui8_recv_ptr, sz_recv_len);
    status::parseResponse(ui16_message_id, pui8_recv_ptr, sz_recv_len);
    monitor::parseResponse(ui16_message_id, pui8_recv_ptr, sz_recv_len);
    event::parseResponse(ui16_message_id, pui8_recv_ptr, sz_recv_len);
    update::parseResponse(ui16_message_id, pui8_recv_ptr, sz_recv_len);
    commands::parseResponse(ui16_message_id, pui8_recv_ptr, sz_recv_len);

}


} // namespace cloud::net
