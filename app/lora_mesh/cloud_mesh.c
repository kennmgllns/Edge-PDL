/*
 * cloud_mesh.c
 *
 */

#include <stdlib.h>
#include "global_defs.h"
#include "system_flags/system_flags.h"
#include "time_utils.h"

#include "edge_payload_cfg.h"
#include "edge_payload.h"

#include "cloud_net_cfg.h"
#include "cloud_net.h"

#include "lora_mesh.h"
#include "lora_mesh_cfg.h"
#include "mesh_cloud_cfg.h"
#include "cloud_mesh.h"


#ifndef ENABLE_MESH_CLOUD_DEBUG
  #undef LOGD
  #define LOGD(...)
#endif


/*
 * Local Variables
 */

static struct {
    struct {
        uint32_t                    u32_id;     // mesh node id
        ep_com_header_st            s_comms;    // comms/device info (comms_id, etc.)
    } s_nodes[K_MESH_CLOUD_MAX_NUM_CHILD_NODES];
    uint8_t                         u8_count;
} s_child_nodes;

typedef struct cloud_payload {
    mesh_comms_payload_header_st    s_hdr;
    uint32_t                        u32_child_id;
    uint32_t                        ms_updated;
    uint8_t                         au8_buf[K_MESH_CLOUD_REPORT_PAYLOAD_MAXIMUM_SIZE];
    uint16_t                        u16_len;  // actual length used
    uint8_t                         u8_idx;   // index on the "as_cloud_payload" array (for debugging only)
} cloud_payload_st;

static cloud_payload_st             as_cloud_payload[K_MESH_CLOUD_REQUEST_QUEUE_SIZE];

static struct {
    enum request_state {
        REQUEST_STATE_IDLE = 0,      // check if there are any queued cloud request
        REQUEST_STATE_SEND_REQ,      // send request to the cloud
        REQUEST_STATE_WAIT_RESP,     // wait response byte from the cloud and forward it to the child node
        REQUEST_STATE_WAIT_ACK,      // wait server-response acknowledge from the child node (to reduce duplicate data)
        REQUEST_STATE_RETRY_DELAY    // wait some time before retrying
    } e_state;
    uint16_t                        u16_msg_id; // cloud net message/token id
    QueueHandle_t                   queue_send; // access to "as_cloud_payload" array
    cloud_payload_st               *ps_req;     // pointer to an "as_cloud_payload" element
    uint32_t                        ms_timeout; // for response-wait & retry-delay
    uint16_t                        u16_num_retries;    // cloud report send retry
    uint16_t                        u16_resp_len;       // received server response length
    uint8_t                         u8_resp_byte;       // received server response byte
    uint16_t                        u8_num_fwd_resp;    // forward server response retry
    bool                            b_ack_received;     // child node acknowledged the server response
} s_cloud_request; // parent-to-cloud request context

/*
 * Private Function Prototypes
 */
static bool getQueuedRequest(void);
static void cbRequestSent(void);
static bool updateChild(uint32_t u32_child_id, const mesh_comms_payload_st *ps_payload);


/*
 * Public Functions
 */
void cloudMeshInit(void)
{
    memset(&s_child_nodes,   0, sizeof(s_child_nodes));
    memset(&s_cloud_request, 0, sizeof(s_cloud_request));
    memset(as_cloud_payload, 0, sizeof(as_cloud_payload));

    // Create queue
    s_cloud_request.queue_send = xQueueCreate(K_MESH_CLOUD_REQUEST_QUEUE_SIZE, sizeof(mesh_data_msg_st *));
    if (NULL == s_cloud_request.queue_send)
    {
        Error_Handler();
    }
    LOGD("mesh-to-cloud queue created");

#if 0 // unit testing only
    for (uint8_t idx = s_child_nodes.u8_count; idx < K_MESH_CLOUD_MAX_NUM_CHILD_NODES; idx++)
    {
        updateChild(rand(), NULL);
    }
#endif
}

void cloudMeshCycle(void)
{
    bool b_req_send = false;

    switch (s_cloud_request.e_state)
    {
    case REQUEST_STATE_IDLE:
        if ((true == cloudNetInterfaceAvailable()) && (true == getQueuedRequest()))
        {
            s_cloud_request.e_state = REQUEST_STATE_SEND_REQ;
        }
        break;

    case REQUEST_STATE_SEND_REQ:
        s_cloud_request.u16_msg_id = cloudNetNewMessageId();

        switch (s_cloud_request.ps_req->s_hdr.u3_endpoint)
        {
        case MESH_COMMS_ENDPOINT_MONITOR:
            b_req_send = cloudNetSendPutRequest(s_cloud_request.u16_msg_id, K_CLOUD_MONITOR_PATH,
                                                s_cloud_request.ps_req->au8_buf, s_cloud_request.ps_req->u16_len);
            break;
        case MESH_COMMS_ENDPOINT_STATUS:
            {
                switch (s_cloud_request.ps_req->s_hdr.u2_method)
                {
                case MESH_COMMS_REQUEST_GET:
                    b_req_send = cloudNetSendGetRequest(s_cloud_request.u16_msg_id, K_CLOUD_STATUS_PATH,
                                                        s_cloud_request.ps_req->au8_buf, s_cloud_request.ps_req->u16_len);
                    break;
                
                case MESH_COMMS_REQUEST_PUT:
                    b_req_send = cloudNetSendPutRequest(s_cloud_request.u16_msg_id, K_CLOUD_STATUS_PATH,
                                                        s_cloud_request.ps_req->au8_buf, s_cloud_request.ps_req->u16_len);
                    break;
                }
            }
            break;

        case MESH_COMMS_ENDPOINT_EVENT:
            b_req_send = cloudNetSendPutRequest(s_cloud_request.u16_msg_id, K_CLOUD_EVENT_PATH,
                                                s_cloud_request.ps_req->au8_buf, s_cloud_request.ps_req->u16_len);
            break;

        case MESH_COMMS_ENDPOINT_UPDATE:
            b_req_send = cloudNetSendGetRequest(s_cloud_request.u16_msg_id, K_CLOUD_UPDATE_PATH,
                                                s_cloud_request.ps_req->au8_buf, s_cloud_request.ps_req->u16_len);
            break;
        
        default:
            b_req_send = false;
            break;
        }

        if (b_req_send)
        {
            s_cloud_request.e_state = REQUEST_STATE_WAIT_RESP;
        }
        else
        {
            s_cloud_request.e_state = REQUEST_STATE_RETRY_DELAY;

        }
        s_cloud_request.u16_resp_len    = 0;
        s_cloud_request.u8_resp_byte    = 0;
        s_cloud_request.b_ack_received  = false;
        s_cloud_request.ms_timeout      = millis();
        break;

    case REQUEST_STATE_WAIT_RESP:
        if (s_cloud_request.u16_resp_len > 1) // got response details (already forwarded to child node once)
        {
            cbRequestSent(); // "GET /st" request - success
        }
        else if (0 != s_cloud_request.u8_resp_byte) // got response byte
        {
            // forward (or resend) server response to the child node
            meshCommsSend(s_cloud_request.ps_req->u32_child_id, s_cloud_request.ps_req->s_hdr.u16_msg_id,
                            &s_cloud_request.u8_resp_byte, 1, MESH_COMMS_RESPONSE, s_cloud_request.ps_req->s_hdr.u3_endpoint, 0, 0);
            if (0 != s_cloud_request.ps_req->s_hdr.u16_msg_id)
            {
                s_cloud_request.u8_num_fwd_resp += 1;
                s_cloud_request.ms_timeout       = millis();
                s_cloud_request.e_state          = REQUEST_STATE_WAIT_ACK;
            }
            else // mesh layer error
            {
                // failed to forward server response
                cbRequestSent(); // discard: just let the child to resend the report.
            }
        }
        else if (millis() - s_cloud_request.ms_timeout >= K_MESH_CLOUD_REQUEST_RESPONSE_TIMEOUT)
        {
            s_cloud_request.ms_timeout = millis();
            s_cloud_request.e_state    = REQUEST_STATE_RETRY_DELAY;
        }
        break;

    case REQUEST_STATE_WAIT_ACK:
        if (true == s_cloud_request.b_ack_received)
        {
            LOGI("response %02X(%u) %08lx %u-%u-%u", s_cloud_request.u8_resp_byte, s_cloud_request.u8_num_fwd_resp, s_cloud_request.ps_req->u32_child_id,
                s_cloud_request.ps_req->s_hdr.u3_endpoint, s_cloud_request.ps_req->u16_len, s_cloud_request.ps_req->s_hdr.u16_msg_id);
            cbRequestSent(); // success
        }
        else if (millis() - s_cloud_request.ms_timeout > K_MESH_CLOUD_RESPONSE_ACK_TIMEOUT)
        {
            if (s_cloud_request.u8_num_fwd_resp > K_MESH_CLOUD_RESPONSE_FWD_MAX_RETRIES)
            {
                // max forward-server-response count
                LOGW("response %02X(nack) %08lx %u-%u-%u", s_cloud_request.u8_resp_byte, s_cloud_request.ps_req->u32_child_id,
                    s_cloud_request.ps_req->s_hdr.u3_endpoint, s_cloud_request.ps_req->u16_len, s_cloud_request.ps_req->s_hdr.u16_msg_id);
                cbRequestSent(); // discard: not acknowledge by the child node (e.g. with older fw)
            }
            else
            {
                s_cloud_request.e_state = REQUEST_STATE_WAIT_RESP; // will retry forwarding server response
                LOGD("resend #%u server resp %02x msg %u", s_cloud_request.u8_num_fwd_resp,
                    s_cloud_request.u8_resp_byte, s_cloud_request.ps_req->s_hdr.u16_msg_id);
            }
        }
        break;

    case REQUEST_STATE_RETRY_DELAY:
        if (millis() - s_cloud_request.ms_timeout > K_MESH_CLOUD_REQUEST_RETRY_DELAY)
        {
            s_cloud_request.e_state         = REQUEST_STATE_IDLE;
            s_cloud_request.u16_num_retries += 1;
            LOGD("retry request %u", s_cloud_request.u16_num_retries);
            if (s_cloud_request.u16_num_retries >= K_MESH_CLOUD_REQUEST_MAX_RETRIES)
            {
                LOGW("max retry");
                cbRequestSent(); // discard na lang
            }
        }
        break;

    default:
        s_cloud_request.e_state = REQUEST_STATE_IDLE;
        break;
    }
}

void cloudMeshParseResponse(uint16_t u16_msg_id, const uint8_t *pu8_buf, size_t sz_len)
{
    if (u16_msg_id == s_cloud_request.u16_msg_id)
    {
        //LOGD("resp=0x%02x len=%lu %.*s", pu8_buf[0], sz_len, sz_len, pu8_buf);

        if ((NULL == s_cloud_request.ps_req) || (0 == sz_len))
        {
            LOGW("unknown payload?");
        }
        else if (1 == sz_len) // got a response byte
        {
            s_cloud_request.u8_resp_byte = pu8_buf[0];
            s_cloud_request.u16_resp_len = 1;
        }
        else // details (i.e. response for "GET /st")
        {
            if (sz_len > MESH_COMMS_PAYLOAD_MAX_SIZE) {
                sz_len = MESH_COMMS_PAYLOAD_MAX_SIZE; // truncate payload
            }
            s_cloud_request.u16_resp_len = sz_len;
            // send command/update details to child node
            meshCommsSend(s_cloud_request.ps_req->u32_child_id, s_cloud_request.ps_req->s_hdr.u16_msg_id,
                            pu8_buf, sz_len, MESH_COMMS_RESPONSE, s_cloud_request.ps_req->s_hdr.u3_endpoint, 0, 0);
        }

    }
}

/* send current time for synching */
bool meshCloudCurrentTime(uint32_t u32_dst_id, const mesh_comms_payload_st *ps_payload)
{
    mesh_node_st    s_child_node;
    char            ac_now_str[20];
    size_t          sz_len;
    int32_t         ts_now;
    bool            b_sync   = false; // is 4G sync ?
    bool            b_status = false;

    if (!getTimeSyncStatus(&b_sync) || !b_sync)
    {
        //LOGD("not yet in sync with server");
    }
    else if (!getRoute(u32_dst_id, &s_child_node))
    {
        LOGW("no route to child %08lx", u32_dst_id);
    }
    else if (!updateChild(u32_dst_id, ps_payload))
    {
        // [busy] max num child nodes
    }
    else if ((false == timeUtilGetRtcNowEpoch(&ts_now)) || 
             (false == timeUtilEpochToString(ac_now_str, sizeof(ac_now_str), &sz_len, ts_now)))
    {
        //
    }
    else if (0 == meshCommsSend(u32_dst_id, ps_payload->s_hdr.u16_msg_id, (uint8_t *)ac_now_str, sz_len,
                                MESH_COMMS_LOCAL, MESH_COMMS_ENDPOINT_TIME, 0, 0))
    {
        //
    }
    else
    {
        //LOGD("now: %s", ac_now_str);
        b_status = true;
    }

    return b_status;
}

bool meshCloudHandleRequest(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload)
{
    static const uint8_t               *ACK         = (const uint8_t *)"ACK\0";
    static const uint8_t               *NACK        = (const uint8_t *)"NACK";
    const mesh_comms_payload_header_st *ps_hdr      = &ps_payload->s_hdr;
    cloud_payload_st                   *ps_req      = NULL;
    uint8_t                             ui8_idx     = 0;
    bool                                b_append    = false;
    bool                                b_duplicate = false;
    bool                                b_status    = false;

    if ((false == updateChild(u32_id_from, NULL)) ||    // limit number of serving child nodes
        (false == cloudNetInterfaceAvailable()))        // don't accept child request if the parent itself is not connected to the cloud
    {
        return false;
    }

    for (ui8_idx = 0; ui8_idx < K_MESH_CLOUD_REQUEST_QUEUE_SIZE; ui8_idx++)
    {
        cloud_payload_st *tmp = &as_cloud_payload[ui8_idx];
        if ((u32_id_from == tmp->u32_child_id) &&
            (0 == memcmp(&tmp->s_hdr, ps_hdr, sizeof(tmp->s_hdr))))
        {
            LOGD("dupicate report %u-%u", tmp->s_hdr.u2_method, tmp->s_hdr.u3_endpoint);
            b_duplicate = true;
            break;
        }
    }

    for (ui8_idx = 0; (false == b_duplicate) && (ui8_idx < K_MESH_CLOUD_REQUEST_QUEUE_SIZE); ui8_idx++)
    {
        cloud_payload_st *tmp = &as_cloud_payload[ui8_idx];
        // if a new request ...
        if (0 == ps_hdr->u2_idx)
        {
            if (0 == tmp->u16_len)
            {
                // found an empty entry
                ps_req = tmp;
                break;
            }
        }
        // if a continuation from a previous report
        else if ((u32_id_from == tmp->u32_child_id) && (tmp->s_hdr.b_partial) &&
                 (ps_hdr->u2_method == tmp->s_hdr.u2_method) &&
                 (ps_hdr->u3_endpoint == tmp->s_hdr.u3_endpoint))
        {
            LOGD("existing report %u-%u", tmp->s_hdr.u2_method, tmp->s_hdr.u3_endpoint);
            /**
             * FIXME: sequential index only for now
             */
            if (ps_hdr->u2_idx == (tmp->s_hdr.u2_idx + 1))
            {
                LOGD("append %u bytes", ps_hdr->u8_length);
                ps_req   = tmp;
                b_append = true;
                break;
            }
            else
            {
                LOGW("not yet supported random sequence");
            }
        }
    }

    if (b_duplicate)
    {
        // just resend ack
        b_status = true;
    }
    else if (NULL == ps_req)
    {
        LOGW("request queue full (%lu)", uxQueueMessagesWaiting(s_cloud_request.queue_send));
    }
    else if (ps_req->u16_len + ps_hdr->u8_length > sizeof(ps_req->au8_buf))
    {
        // should not go here
        LOGE("not enough payload buffer (%u > %lu)", ps_req->u16_len + ps_hdr->u8_length, sizeof(ps_req->au8_buf));
    }
    else
    {
        // copy or append data
        memcpy(&ps_req->au8_buf[ps_req->u16_len], ps_payload->au8_data, ps_hdr->u8_length);
        ps_req->u16_len      += ps_hdr->u8_length;
        
        // copy info
        memcpy(&ps_req->s_hdr, ps_hdr, sizeof(ps_req->s_hdr));
        ps_req->u32_child_id = u32_id_from;
        ps_req->ms_updated   = millis();
        ps_req->u8_idx       = ui8_idx;
#if 0
        switch (ps_hdr->u3_endpoint)
        {
        case MESH_COMMS_ENDPOINT_MONITOR:
            LOGD("monitor data %u bytes (partial %u)", ps_req->u16_len, ps_hdr->b_partial);
            break;
        case MESH_COMMS_ENDPOINT_STATUS:
            LOGD("status data %u bytes", ps_req->u16_len);
            break;
        case MESH_COMMS_ENDPOINT_EVENT:
            LOGD("event data %u bytes", ps_req->u16_len);
            break;
        case MESH_COMMS_ENDPOINT_UPDATE:
            LOGD("update request from %08lx", u32_id_from);
            break;
        default:
            LOGE("not supported endpoint %u", ps_hdr->u3_endpoint);
            break;
        }
#endif
        if (b_append)
        {
            // already in the queue
            b_status = true;
        }
        else if (pdTRUE == xQueueSend(s_cloud_request.queue_send, &ps_req, 500UL))
        {
            LOGD("request %u queued %u bytes (idx=%u ep=%u)", ps_hdr->u16_msg_id, ps_req->u16_len, ps_req->u8_idx, ps_hdr->u3_endpoint);
            b_status = true;
        }
        else
        {
            LOGW("request queue failed");
            memset(ps_req, 0, sizeof(cloud_payload_st));
        }
    }

    // send ACK or NACK
    meshCommsSend(u32_id_from, ps_hdr->u16_msg_id, b_status ? ACK : NACK, sizeof(NACK),
                                MESH_COMMS_LOCAL, ps_hdr->u3_endpoint, 0, 0);

    return b_status;
}

bool meshCloudParseResponseAck(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload)
{
    if ((NULL == s_cloud_request.ps_req) || (u32_id_from != s_cloud_request.ps_req->u32_child_id))
    {
        // unexpected child node
    }
    else if ((ps_payload->s_hdr.u16_msg_id != s_cloud_request.ps_req->s_hdr.u16_msg_id) ||
             (ps_payload->s_hdr.u3_endpoint != s_cloud_request.ps_req->s_hdr.u3_endpoint))
    {
        // unexpected message id or endpoint
    }
    else
    {
        s_cloud_request.b_ack_received = (0 == memcmp(ps_payload->au8_data, "ACK", 3));
    }

    return s_cloud_request.b_ack_received;
}


/*
 * Private Functions
 */

static bool getQueuedRequest(void)
{
    cloud_payload_st *ps_req = NULL;

    if (pdTRUE != xQueuePeek(s_cloud_request.queue_send, &ps_req, 0))
    {
        // no queued request
    }
    else if (NULL == ps_req)
    {
        LOGE("invalid request");
        (void)xQueueReceive(s_cloud_request.queue_send, &ps_req, 0);
    }
    else if (true == ps_req->s_hdr.b_partial)
    {
        //LOGD("not yet complete payload");
        if (millis() - ps_req->ms_updated > K_MESH_CLOUD_REQUEST_INCOMPLETE_TIMEOUT)
        {
            LOGW("discard outdated request");
            (void)xQueueReceive(s_cloud_request.queue_send, &ps_req, 10);
            memset(ps_req, 0, sizeof(cloud_payload_st));
        }
        else if (uxQueueMessagesWaiting(s_cloud_request.queue_send) > 1)
        {
            // if single request pa lang, do nothing
            // else, transfer to the back of the queue
        }
        else if (pdTRUE != xQueueReceive(s_cloud_request.queue_send, &ps_req, 10))
        {
            LOGD("failed to temporary remove queue");
        }
        else if (pdTRUE != xQueueSend(s_cloud_request.queue_send, &ps_req, 2000UL))
        {
            LOGD("failed to transfer queue");
        }
        else
        {
            //LOGD("re-queued report %u-%u", ps_req->s_hdr.u2_method, ps_req->s_hdr.u3_endpoint);
        }
    }
    else
    {
        // got complete request
        s_cloud_request.ps_req = ps_req;
        return true;
    }

    return false;
}

static void cbRequestSent(void)
{
    cloud_payload_st *ps_req = NULL;

    if (pdTRUE != xQueueReceive(s_cloud_request.queue_send, &ps_req, 10))
    {
        LOGW("failed to clear request");
    }
    else if (NULL == ps_req)
    {
        //LOGW("invalid request");
    }
    else
    {
        LOGD("remove request %u (%u-%lu)",ps_req->s_hdr.u16_msg_id , ps_req->u8_idx, uxQueueMessagesWaiting(s_cloud_request.queue_send));
        taskENTER_CRITICAL();
        memset(ps_req, 0, sizeof(cloud_payload_st));
        taskEXIT_CRITICAL();
    }
    s_cloud_request.e_state         = REQUEST_STATE_IDLE;
    s_cloud_request.u16_num_retries = 0;
    s_cloud_request.u8_num_fwd_resp = 0;
}

static bool updateChild(uint32_t u32_child_id, const mesh_comms_payload_st *ps_payload)
{
    uint8_t u8_idx;
    bool    b_status = false;

    // find existing child
    for (u8_idx = 0; u8_idx < s_child_nodes.u8_count; u8_idx++)
    {
        if (u32_child_id == s_child_nodes.s_nodes[u8_idx].u32_id)
        {
            //LOGD("refresh child %u: %08lx", u8_idx + 1, u32_child_id);
            s_child_nodes.s_nodes[u8_idx].s_comms.e_dev_type = millis();
            b_status = true;
            break;
        }
    }

    // remove an inactive child
    for (u8_idx = 0; !b_status && (u8_idx < s_child_nodes.u8_count); u8_idx++)
    {
        if (millis() - s_child_nodes.s_nodes[u8_idx].s_comms.si32_timestamp > MESH_NODE_INACTIVE_TIMEOUT)
        {
            LOGD("remove child %u: %08lx", u8_idx + 1, s_child_nodes.s_nodes[u8_idx].u32_id);
            // overwrite info
            memmove(&s_child_nodes.s_nodes[u8_idx], &s_child_nodes.s_nodes[u8_idx + 1],
                    sizeof(s_child_nodes.s_nodes[0]) * (K_MESH_CLOUD_MAX_NUM_CHILD_NODES - u8_idx - 1));
            s_child_nodes.u8_count--;
            s_child_nodes.s_nodes[s_child_nodes.u8_count].u32_id = 0;
            s_child_nodes.s_nodes[s_child_nodes.u8_count].s_comms.si32_timestamp = 0;
            break;
        }
    }

    // try add on an empty slot
    if (!b_status && (s_child_nodes.u8_count < K_MESH_CLOUD_MAX_NUM_CHILD_NODES))
    {
        ep_com_header_st *ps_comms = &s_child_nodes.s_nodes[s_child_nodes.u8_count].s_comms;
        const uint8_t    *pu8_id   = ps_comms->s_dev_id.aui8_id; 
        // if comes from a sync request ...
        if ((NULL != ps_payload) && (MESH_COMMS_ENDPOINT_TIME == ps_payload->s_hdr.u3_endpoint) &&
            (sizeof(ep_com_header_st) == ps_payload->s_hdr.u8_length))
        {
            memcpy(ps_comms, ps_payload->au8_data, sizeof(ep_com_header_st)); // store device info
        }
        // add new child
        s_child_nodes.s_nodes[s_child_nodes.u8_count].u32_id = u32_child_id;
        ps_comms->si32_timestamp = millis();
        s_child_nodes.u8_count++;

        LOGI("new child %u: %02x-%02x%02x%02x%02x%02x%02x (node id %08lx)", s_child_nodes.u8_count,
            ps_comms->e_dev_type, pu8_id[0], pu8_id[1], pu8_id[2], pu8_id[3], pu8_id[4], pu8_id[5], u32_child_id);

        b_status = true;
    }

    if (!b_status)
    {
        LOGD("parent node busy");
    }

    return b_status;
}
