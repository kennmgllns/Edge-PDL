
/*
 * mesh_cloud.c
 *
 */
#include "global_defs.h"
#include "system_flags.h"
#include "time_utils.h"
#include "heartbeat.h"

#include "edge_payload_cfg.h"
#include "edge_payload.h"

#include "data_logging.h"

#include "lora_mesh.h"
#include "lora_mesh_cfg.h"
#include "mesh_cloud_cfg.h"


#ifndef ENABLE_MESH_CLOUD_DEBUG
  #undef LOGD
  #define LOGD(...)
#endif

/*
 * Local Variables
 */

static struct {
    enum sync_state {
        SYNC_STATE_IDLE = 0,
        SYNC_STATE_SEND_REQ,
        SYNC_STATE_WAIT_RESP,
        SYNC_STATE_RETRY_DELAY
    } e_state;
    mesh_node_st                    s_parent;       // parent node for "bridged" communications to the cloud
    uint32_t                        u32_ignore_id;  // node id to ignore in scanning of parent node
    uint32_t                        ms_start;       // timestamp of request send
    uint16_t                        u16_msg_id;     // comms message id
    uint16_t                        u16_num_retries;
    bool                            b_status;       // sync result
    bool                            b_info_ready;
} s_mesh_sync;

static struct {
    enum report_type {
        REPORT_TYPE_NONE = 0,
        REPORT_TYPE_MONITOR,
        REPORT_TYPE_STATUS,
        REPORT_TYPE_EVENT
    } e_type;
    enum report_state {
        REPORT_STATE_IDLE = 0,      // check if has a route to a 4G-connected node
        REPORT_STATE_DATA,          // check if there are any queued data
        REPORT_STATE_SEND_REQ,      // send report via mesh
        REPORT_STATE_WAIT_ACK,      // wait acknowledge from the parent 4G-node
        REPORT_STATE_WAIT_RESP,     // wait cloud server response, and then send acknowledge to parent
        REPORT_STATE_RETRY_DELAY    // wait some time before retrying
    } e_state;
    uint32_t                        ms_timeout; // for response-wait & retry-delay
    uint16_t                        u16_num_retries;
    bool                            b_ack_received; // parent node ack (report data was queued)
    bool                            b_ack_status;   // queue ok
    uint8_t                         u8_resp_byte;   // received server response byte (forwarded by parent node)
    uint8_t                         u8_num_errors;  // response error count
    uint16_t                        u16_prev_msg_id;// previously acknowledged report message id
} s_mesh_report; // mesh-to-parent report context

static struct {
    mesh_comms_payload_header_st    s_hdr;
    uint8_t                         au8_buf[K_MESH_CLOUD_REPORT_PAYLOAD_MAXIMUM_SIZE];
    size_t                          sz_len;  // actual length used
    uint32_t                        u32_epoch; // report timestamp (for debugging only)
} s_mesh_payload; // coap-reports payload from data-logging queues

/*
 * Private Function Prototypes
 */
static bool parseTimePayload(const char *pc_time, size_t sz_len);
static void queueNextParent(void);
static bool getQueuedData(void);
static void cbDataSent(void);
static bool sendReportData(uint32_t u32_dest_id);


/*
 * Public Functions
 */

void meshCloudInit(bool b_full)
{
    memset(&s_mesh_payload, 0, sizeof(s_mesh_payload));

    if (b_full)
    {
        memset(&s_mesh_sync,   0, sizeof(s_mesh_sync));
        memset(&s_mesh_report, 0, sizeof(s_mesh_report));
    }
    else
    {
        s_mesh_sync.e_state   = SYNC_STATE_IDLE;
        s_mesh_report.e_type  = REPORT_TYPE_NONE;
        s_mesh_report.e_state = REPORT_STATE_IDLE;
    }

    (void)setMeshConnStatus(FALSE);
}

bool meshCloudCycle(void)
{
    //LOGD("%s()", __func__);
    mesh_node_st    s_tmp_node;
    bool            b_status = true;

    switch (s_mesh_report.e_state)
    {
    case REPORT_STATE_IDLE:
        if (0 == s_mesh_sync.s_parent.u32_id)
        {
            // no parent yet, needs sync
            LOGD("no parent node");
            b_status = false;
        }
        else if (!getRoute(s_mesh_sync.s_parent.u32_id, &s_tmp_node))
        {
            LOGW("no route to parent %08lx", s_mesh_sync.s_parent.u32_id);
            queueNextParent();
            s_mesh_diag.u32_sync_error++;
            b_status = false;
        }
        else
        {
            //LOGD("found parent node %08lx", s_parent_node.u32_id);
            s_mesh_report.e_state = REPORT_STATE_DATA;
        }
        break;

    case REPORT_STATE_DATA:
        if (getQueuedData())
        {
            s_mesh_report.e_state = REPORT_STATE_SEND_REQ;
        }
        else
        {
            //LOGD("no pending reports");
            s_mesh_report.e_type    = REPORT_TYPE_NONE;
            s_mesh_report.e_state   = REPORT_STATE_IDLE;
        }
        break;

    case REPORT_STATE_SEND_REQ:
        s_mesh_report.b_ack_received = false;
        s_mesh_report.b_ack_status   = false;
        s_mesh_report.u8_resp_byte   = 0;
        if (sendReportData(s_mesh_sync.s_parent.u32_id))
        {
            s_mesh_report.e_state = REPORT_STATE_WAIT_ACK;
        }
        else
        {
            s_mesh_report.e_state = REPORT_STATE_RETRY_DELAY;
        }
        s_mesh_report.ms_timeout = millis();
        break;

    case REPORT_STATE_WAIT_ACK:
        if (true == s_mesh_report.b_ack_received)
        {
            if (true == s_mesh_report.b_ack_status)
            {
                //  if partial, proceed to the next segment
                if (true == s_mesh_payload.s_hdr.b_partial)
                {
                    LOGI("report %u... (%u-%u-%lu)", s_mesh_payload.s_hdr.u16_msg_id, s_mesh_payload.s_hdr.u3_endpoint,
                        s_mesh_payload.s_hdr.u8_length, s_mesh_payload.u32_epoch);
                    cbDataSent();
                }
                else // else, wait for server response to be forwared by the parent node
                {
                    s_mesh_report.e_state    = REPORT_STATE_WAIT_RESP;
                    s_mesh_report.ms_timeout = millis();
                }
            }
            else // nack, e.g. queue full
            {
                LOGW("ack error");
                s_mesh_diag.u32_ack_error++;
                (void)setMeshConnStatus(FALSE);
                memset(&s_mesh_payload, 0, sizeof(s_mesh_payload)); // will resend partial reports if there's any
                b_status                 = false;
                s_mesh_report.ms_timeout = millis();
                s_mesh_report.e_type     = REPORT_TYPE_NONE;
                s_mesh_report.e_state    = REPORT_STATE_RETRY_DELAY;
            }
        }
        else if (millis() - s_mesh_report.ms_timeout > K_MESH_CLOUD_REPORT_ACK_TIMEOUT)
        {
            LOGW("ack timeout %u-%u", s_mesh_payload.s_hdr.u16_msg_id, s_mesh_payload.s_hdr.b_partial);
            s_mesh_diag.u32_ack_error++;
            (void)setMeshConnStatus(FALSE);
            s_mesh_report.ms_timeout = millis();
            s_mesh_report.e_state    = REPORT_STATE_RETRY_DELAY;
        }
        break;

    case REPORT_STATE_WAIT_RESP:
#if 0 // test only
        if (rand() & 1) { s_mesh_report.u8_resp_byte = 0; }
#endif
        if (0 != s_mesh_report.u8_resp_byte)
        {
            // got a response from the server via the parent node
            (void)setMeshConnStatus(TRUE);

            // send ACK, regardless of the response byte value
            meshCommsSend(s_mesh_sync.s_parent.u32_id, s_mesh_payload.s_hdr.u16_msg_id, (const uint8_t *)"ACK", 3,
                            MESH_COMMS_LOCAL, s_mesh_payload.s_hdr.u3_endpoint, 0, 0);
            s_mesh_report.u16_prev_msg_id = s_mesh_payload.s_hdr.u16_msg_id;

            // if response ok ...
            if ((s_mesh_report.u8_resp_byte >= EP_SERVER_RESP_OK) && (s_mesh_report.u8_resp_byte <= EP_SERVER_RESP_OPERATION_REQ))
            {
                LOGI("report %u ok (%u-%u-%lu)", s_mesh_payload.s_hdr.u16_msg_id, s_mesh_payload.s_hdr.u3_endpoint,
                    s_mesh_payload.s_hdr.u8_length, s_mesh_payload.u32_epoch);
                cbDataSent();
            }
            else // response error
            {
                s_mesh_report.u8_num_errors += 1;
                s_mesh_diag.u32_rsp_error++;
                LOGW("report %u error (%u-%u-%lu) 0%02x #%u", s_mesh_payload.s_hdr.u16_msg_id, s_mesh_payload.s_hdr.u3_endpoint,
                    s_mesh_payload.s_hdr.u8_length, s_mesh_payload.u32_epoch, s_mesh_report.u8_resp_byte, s_mesh_report.u8_num_errors);
                if (s_mesh_report.u8_num_errors >= K_MESH_CLOUD_REPORT_MAX_RESP_ERRORS)
                {
                    LOGE("max errors");
                    cbDataSent(); // discard na lang (e.g. malformed data logging payload or unsupported parameter)
                }
                else // retry, maybe the server is temporarily down
                {
                    s_mesh_report.ms_timeout = millis();
                    s_mesh_report.e_state    = REPORT_STATE_RETRY_DELAY;
                }
            }
        }
        else if (millis() - s_mesh_report.ms_timeout > K_MESH_CLOUD_REQUEST_RESPONSE_TIMEOUT)
        {
            LOGW("response timeout %u", s_mesh_payload.s_hdr.u16_msg_id);
            s_mesh_diag.u32_rsp_error++;
            (void)setMeshConnStatus(FALSE);
            s_mesh_report.ms_timeout = millis();
            s_mesh_report.e_state    = REPORT_STATE_RETRY_DELAY;
        }
        break;

    case REPORT_STATE_RETRY_DELAY:
        if (millis() - s_mesh_report.ms_timeout > K_MESH_CLOUD_REQUEST_RESPONSE_TIMEOUT /*K_MESH_CLOUD_REPORT_RETRY_DELAY*/)
        {
            s_mesh_report.e_state         = REPORT_STATE_IDLE;
            s_mesh_report.u16_num_retries += 1;
            LOGW("retry %u report %u", s_mesh_report.u16_num_retries, s_mesh_payload.s_hdr.u16_msg_id);
            if (s_mesh_report.u16_num_retries > K_MESH_CLOUD_REPORT_SYNC_RETRIES)
            {
                LOGD("will try sending report on the another parent");
                queueNextParent();
                b_status = false;
#if 0 // fixme: adjust max_retires for 60-day data retention
                if (s_mesh_report.u16_num_retries > K_MESH_CLOUD_REPORT_MAX_RETRIES)
                {
                    LOGE("max report-send retries");
                    cbDataSent(); // discard na lang
                }
#endif
            }
        }
        break;

    default:
        s_mesh_report.e_state = REPORT_STATE_IDLE;
        break;
    }

    return b_status;
}

bool meshCloudParseReportAck(const mesh_comms_payload_st *ps_payload)
{
    if (ps_payload->s_hdr.u16_msg_id == s_mesh_payload.s_hdr.u16_msg_id)
    {
        s_mesh_report.b_ack_received = true;
        s_mesh_report.b_ack_status   = (0 == memcmp(ps_payload->au8_data, "ACK", 3));
        LOGD("parent resp (%u) %.*s", s_mesh_report.b_ack_status, ps_payload->s_hdr.u8_length, ps_payload->au8_data);
    }
    return s_mesh_report.b_ack_status;
}

void meshCloudParseServerResponse(const mesh_comms_payload_st *ps_payload)
{
    if (ps_payload->s_hdr.u16_msg_id == s_mesh_payload.s_hdr.u16_msg_id)
    {
        LOGD("server resp (%u) %.*s", ps_payload->s_hdr.u8_length, ps_payload->s_hdr.u8_length, ps_payload->au8_data);
        if (1 == ps_payload->s_hdr.u8_length) // got response byte
        {
            s_mesh_report.u8_resp_byte = ps_payload->au8_data[0];

            // in case of mesh report was previously ack'ed but we did not received properly
            if ((REPORT_STATE_WAIT_ACK == s_mesh_report.e_state) || (REPORT_STATE_RETRY_DELAY == s_mesh_report.e_state))
            {
                LOGD("missed parent ack %u", s_mesh_payload.s_hdr.u16_msg_id);
                s_mesh_report.e_state        = REPORT_STATE_WAIT_RESP;
                s_mesh_report.b_ack_received = true;
                s_mesh_report.b_ack_status   = true;
            }
        }
    }
    // else, resend ACK - in case parent didn't receive our server-response-ACK
    else if (ps_payload->s_hdr.u16_msg_id == s_mesh_report.u16_prev_msg_id)
    {
        LOGD("resend ack %u", s_mesh_report.u16_prev_msg_id);
        meshCommsSend(s_mesh_sync.s_parent.u32_id, s_mesh_report.u16_prev_msg_id, (const uint8_t *)"ACK", 3,
                        MESH_COMMS_LOCAL, ps_payload->s_hdr.u3_endpoint, 0, 0);
    }
    else
    {
        //LOGD("not handled server resp (%u != %u) %.*s", ps_payload->s_hdr.u16_msg_id, s_mesh_payload.s_hdr.u16_msg_id, ps_payload->s_hdr.u8_length, ps_payload->au8_data);
    }
}

uint32_t meshCloudGetParentId(void)
{
    return s_mesh_sync.s_parent.u32_id;
}

bool meshCloudGetInfo(mesh_cloud_info_st *ps_info)
{
    if ((true == s_mesh_sync.b_info_ready) && (true == s_mesh_sync.b_status) && (NULL != ps_info))
    {
        s_mesh_sync.b_info_ready        = false;
        ps_info->s_node.ui32_parent_id  = s_mesh_sync.s_parent.u32_id;
        ps_info->s_node.ui32_hop_id     = s_mesh_sync.s_parent.u32_first_hop;
        ps_info->s_node.i16_rssi        = s_mesh_sync.s_parent.i16_rssi;
        return true;
    }
    return false;
}

uint32_t meshCloudGetLastSync(void)
{
    return s_mesh_sync.ms_start;
}

int meshCloudSync(bool b_forced)
{
    ep_com_header_st    s_comms;
    int                 result = 0;

#if 0 // debugging only: hardcode the parent node
    getRoute(0x1406540D, &s_mesh_sync.s_parent);
#endif

    switch (s_mesh_sync.e_state)
    {
    case SYNC_STATE_IDLE:
        if ((false == b_forced) && (0 != s_mesh_sync.s_parent.u32_id) && (true == s_mesh_sync.b_status))
        {
            result = 1; // still in-sync with previous parent
        }
        else if (!getCloudNode(&s_mesh_sync.s_parent, s_mesh_sync.u32_ignore_id))
        {
            result = -1; // no online peer node
        }
        else
        {
            LOGI("parent node %08lx", s_mesh_sync.s_parent.u32_id);
            (void)setMeshConnStatus(FALSE); // reset conn status
            s_mesh_sync.e_state = SYNC_STATE_SEND_REQ;
        }
        break;

    case SYNC_STATE_SEND_REQ:
        (void)edgePayloadInitComHeader(&s_comms, 0);
        s_mesh_sync.u16_msg_id  = meshCommsSend(s_mesh_sync.s_parent.u32_id, 0, (const uint8_t *)&s_comms, sizeof(s_comms),
                                    MESH_COMMS_REQUEST_GET, MESH_COMMS_ENDPOINT_TIME, 0, 0);
        s_mesh_sync.ms_start    = millis();
        s_mesh_sync.b_status    = false;
        s_mesh_sync.e_state     = (s_mesh_sync.u16_msg_id > 0) ? SYNC_STATE_WAIT_RESP : SYNC_STATE_RETRY_DELAY;
        break;

    case SYNC_STATE_WAIT_RESP:
        if (0 == s_mesh_sync.u16_msg_id)
        {
            if (true == s_mesh_sync.b_status)
            {
                s_mesh_sync.e_state      = SYNC_STATE_IDLE;
                s_mesh_sync.b_info_ready = true;
                result  = 1;
            }
            else
            {
                LOGW("%s: response error", __func__);
                s_mesh_diag.u32_sync_error++;
                result                  = -1;
                s_mesh_sync.ms_start    = millis();
                s_mesh_sync.e_state     = SYNC_STATE_RETRY_DELAY;
            }
        }
        else if (millis() - s_mesh_sync.ms_start > K_MESH_CLOUD_REPORT_ACK_TIMEOUT)
        {
            LOGW("%s: response timeout", __func__);
            s_mesh_diag.u32_sync_error++;
            s_mesh_sync.ms_start    = millis();
            s_mesh_sync.e_state     = SYNC_STATE_RETRY_DELAY;
        }
        break;

    case SYNC_STATE_RETRY_DELAY:
        if (millis() - s_mesh_sync.ms_start > K_MESH_CLOUD_REPORT_ACK_TIMEOUT)
        {
            s_mesh_sync.e_state          = SYNC_STATE_IDLE;
            s_mesh_sync.u16_num_retries += 1;
            if (s_mesh_sync.u16_num_retries > 4 /*K_MESH_CLOUD_REPORT_SYNC_RETRIES*/)
            {
                queueNextParent();
                result = -1;
            }
        }
        break;

    default:
        s_mesh_sync.e_state = SYNC_STATE_IDLE;
        break;
    }

    return result;
}

bool meshCloudParseSyncResponse(const mesh_comms_payload_st *ps_payload)
{
    const mesh_comms_payload_header_st  *ps_hdr = &ps_payload->s_hdr;

    if (ps_hdr->u16_msg_id == s_mesh_sync.u16_msg_id)
    {
        LOGD("%s(%u, %.*s)", __func__, ps_hdr->u8_length, ps_hdr->u8_length, ps_payload->au8_data);

        s_mesh_sync.b_status    = parseTimePayload((const char *)ps_payload->au8_data, ps_hdr->u8_length);
        s_mesh_sync.u16_msg_id  = 0; // indicate as done
    }

    return s_mesh_sync.b_status;
}


/*
 * Private Functions
 */

static bool parseTimePayload(const char *pc_time, size_t sz_len)
{
    rtc_date_st s_date;
    rtc_time_st s_time;
    bool        b_4g_sync;
    bool        b_lora_sync;
    uint32_t    ms_rtt;
    int32_t     si32_sync_timestamp;
    bool        b_status = false;

    get4gTimeSyncStatus(&b_4g_sync);
    getLoraTimeSyncStatus(&b_lora_sync);
    if( (true == b_4g_sync) || (true == b_lora_sync))
    {
        //already sync at the server using 4g connection
        return true;
    }

    if (false == timeUtilStringToEpoch(&si32_sync_timestamp, pc_time, sz_len))
    {
        /* timestamp parse error */
    }
    else
    {
        /* approximate time sync compensation from round-trip time */
        ms_rtt = (millis() - s_mesh_sync.ms_start);

        //LOGD("round trip : %dms",ms_rtt);
        if(ms_rtt < (5 * 1000 ))
        {
            ms_rtt = (ms_rtt >> 1);
            si32_sync_timestamp += (int32_t)((ms_rtt) / (1000));
            if (true == timeUtilEpochToRtcDateTime(&s_date, &s_time, si32_sync_timestamp))
            {
                b_status = syncTime(&s_date, &s_time, si32_sync_timestamp);
                setLoraTimeSyncStatus(true);
                LOGI("time sync %ld (%d, %lums)", si32_sync_timestamp, b_status, ms_rtt);
            }
        }
        else
        {
            if(b_4g_sync)
            {
                LOGD("failed to sync using LoRa use 4g Sync time");
                b_status = true;
            }
            else
            {
                LOGW("time sync round-trip delay error : %dms",ms_rtt);
            }
        }

    }

    return b_status;
}

static void queueNextParent(void)
{
    s_mesh_sync.u32_ignore_id   = s_mesh_sync.s_parent.u32_id; // try next parent
    LOGD("%s: ignore node %08lx", __func__, s_mesh_sync.u32_ignore_id);
    s_mesh_sync.s_parent.u32_id = 0;
    s_mesh_sync.u16_num_retries = 0;
    s_mesh_sync.b_status        = false;
}

static bool getQueuedData(void)
{
    static int  next_type   = REPORT_TYPE_NONE;
    bool        b_status    = false;

    if ((REPORT_TYPE_NONE != s_mesh_report.e_type) && (0 != s_mesh_payload.sz_len))
    {
        LOGD("pending report %d (%lu bytes)", s_mesh_report.e_type, s_mesh_payload.sz_len);
        b_status = true;
    }
    else
    {
        // clear struct for new report data
        memset(&s_mesh_payload, 0, sizeof(s_mesh_payload));

        switch (next_type)
        {
        case REPORT_TYPE_MONITOR:
            b_status = getQueuedMonitorData(s_mesh_payload.au8_buf, sizeof(s_mesh_payload.au8_buf), &s_mesh_payload.sz_len);
            break;

        case REPORT_TYPE_STATUS:
            b_status = getQueuedStatusData(s_mesh_payload.au8_buf, sizeof(s_mesh_payload.au8_buf), &s_mesh_payload.sz_len);
            break;

        case REPORT_TYPE_EVENT:
            b_status = getQueuedEventData(s_mesh_payload.au8_buf, sizeof(s_mesh_payload.au8_buf), &s_mesh_payload.sz_len);
            break;
        }

        if (b_status)
        {
            s_mesh_report.e_type = next_type;
            // read report timestamp
            memcpy(&s_mesh_payload.u32_epoch, &s_mesh_payload.au8_buf[9], sizeof(uint32_t));
#if 0 // random delay before sending data
            // add random delay before sending data
            uint32_t ms_tx_delay = 0; // random send delay
            getRandomNumber(&ms_tx_delay);
            ms_tx_delay &= 0xFFF; // 0 to ~4sec delay
            LOGD("report type %d len %lu ts %lu (delay %lums)", s_mesh_report.e_type, s_mesh_payload.sz_len, s_mesh_payload.u32_epoch, ms_tx_delay);
            delayMs(ms_tx_delay);
#endif
        }
        else
        {
            s_mesh_report.e_type = REPORT_TYPE_NONE;
            //LOGD("no pending type %d reports", next_type);
        }

        if (++next_type > REPORT_TYPE_EVENT)
        {
            next_type = REPORT_TYPE_MONITOR;
        }
    }

    return b_status;
}

static void cbDataSent(void)
{
    if (s_mesh_payload.s_hdr.b_partial)
    {
        LOGD("move buffer %u %lu - %u", s_mesh_payload.s_hdr.u2_idx + 1, s_mesh_payload.sz_len, s_mesh_payload.s_hdr.u8_length);
        memmove(s_mesh_payload.au8_buf, &s_mesh_payload.au8_buf[s_mesh_payload.s_hdr.u8_length], s_mesh_payload.sz_len - s_mesh_payload.s_hdr.u8_length);
        s_mesh_payload.sz_len -= s_mesh_payload.s_hdr.u8_length;
        s_mesh_payload.s_hdr.u2_idx++;
    }
    else
    {
        LOGD("data sent");
        switch (s_mesh_report.e_type)
        {
        case REPORT_TYPE_MONITOR:
            queuedMonitorSent();
            break;
        case REPORT_TYPE_STATUS:
            queuedStatusSent();
            break;
        case REPORT_TYPE_EVENT:
            queuedEventSent();
            break;
        default: // should not go here
            break;
        }
        s_mesh_report.e_type = REPORT_TYPE_NONE;
    }

    s_mesh_payload.s_hdr.u16_msg_id = 0; // will generate new id
    s_mesh_report.u16_num_retries   = 0; // reset retry counter
    s_mesh_report.u8_num_errors     = 0; // reset error counter
    s_mesh_report.e_state           = REPORT_STATE_IDLE;
}

static bool sendReportData(uint32_t u32_dest_id)
{
    s_mesh_payload.s_hdr.u2_method = MESH_COMMS_REQUEST_PUT;

    switch (s_mesh_report.e_type)
    {
    case REPORT_TYPE_MONITOR:
        LOGD("monitor %lu", s_mesh_payload.u32_epoch);
        s_mesh_payload.s_hdr.u3_endpoint = MESH_COMMS_ENDPOINT_MONITOR;
        break;
    case REPORT_TYPE_STATUS:
        LOGD("status tag %u %lu: %.*s", s_mesh_payload.au8_buf[14], s_mesh_payload.u32_epoch, s_mesh_payload.au8_buf[15], &s_mesh_payload.au8_buf[16]);
        s_mesh_payload.s_hdr.u3_endpoint = MESH_COMMS_ENDPOINT_STATUS;
        break;
    case REPORT_TYPE_EVENT:
        LOGD("event type %d %lu", s_mesh_payload.au8_buf[14], s_mesh_payload.u32_epoch);
        s_mesh_payload.s_hdr.u3_endpoint = MESH_COMMS_ENDPOINT_EVENT;
        break;
    default:
        LOGE("unknown type %d", s_mesh_report.e_type);
        return false;
    }

    if (s_mesh_payload.sz_len > K_MESH_CLOUD_REPORT_PAYLOAD_SEGMENT_SIZE)
    {
        LOGD("partial payload (excess %lu)", s_mesh_payload.sz_len - K_MESH_CLOUD_REPORT_PAYLOAD_SEGMENT_SIZE);
        s_mesh_payload.s_hdr.u8_length = K_MESH_CLOUD_REPORT_PAYLOAD_SEGMENT_SIZE;
        s_mesh_payload.s_hdr.b_partial = true;
    }
    else
    {
        s_mesh_payload.s_hdr.u8_length = (uint8_t)s_mesh_payload.sz_len;
        s_mesh_payload.s_hdr.b_partial = false;
    }

    s_mesh_payload.s_hdr.u16_msg_id = meshCommsSend(u32_dest_id, s_mesh_payload.s_hdr.u16_msg_id,
                                                s_mesh_payload.au8_buf, s_mesh_payload.s_hdr.u8_length,
                                                s_mesh_payload.s_hdr.u2_method, s_mesh_payload.s_hdr.u3_endpoint,
                                                s_mesh_payload.s_hdr.u2_idx, s_mesh_payload.s_hdr.b_partial);

    return (s_mesh_payload.s_hdr.u16_msg_id > 0);
}
