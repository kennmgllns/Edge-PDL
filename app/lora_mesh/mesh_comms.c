/*
 * mesh_comms.c
 *
 */

#include "global_defs.h"

#include "edge_payload_cfg.h"
#include "edge_payload.h"

#include "cloud_net_cfg.h"
#include "cloud_net.h"
#include "cloud_mesh.h"

#include "system_flags_cfg.h"
#include "system_flags.h"

#include "lora_mesh.h"
#include "lora_mesh_cfg.h"


#ifndef ENABLE_MESH_COMMS_DEBUG
  #undef LOGD
  #define LOGD(...)
#endif


/*
 * Global Variables
 */
mesh_events_st  s_mesh_events;

/*
 * Local Constants
 */
typedef enum
{
    MESH_COMMS_STATE_INIT = 0,  // check cloud/4G connectivity
    MESH_COMMS_STATE_SYNC,      // sync to a node with 4G connectiion
    MESH_COMMS_STATE_CLOUD,     // send cloud data via mesh
    MESH_COMMS_STATE_IDLE
} mesh_comms_state_et;

/*
 * Local Variables
 */

static bool                     b_nodes_list_changed = false;
static bool                     b_force_sync = false;

static mesh_comms_state_et      e_comms_state;
static uint32_t                 ms_last_cloud_connected;
static uint16_t                 u16_message_counter;

/*
 * Private Function Prototypes
 */
static void onLoraData(uint32_t fromID, uint8_t *rxPayload, uint16_t rxSize, int16_t rxRssi, int8_t rxSnr);
static void onNodesListChange(void);

static void processClientRequest(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload);
static void processServerResponse(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload);
static void processLocalMessage(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload);

/*
 * Public Functions
 */
void meshCommsInit(void)
{
    meshCloudInit(true);
    meshCommandInit();
    meshUpdateInit();

    memset(&s_mesh_events, 0, sizeof(s_mesh_events));
    s_mesh_events.DataAvailable = onLoraData;
    s_mesh_events.NodesListChanged = onNodesListChange;

    e_comms_state = MESH_COMMS_STATE_INIT;
    ms_last_cloud_connected = 0;
    u16_message_counter     = 0;
}

void meshCommsCycle(void)
{
    int result;
    //LOGD("%s", __func__);

    switch (e_comms_state)
    {
    case MESH_COMMS_STATE_INIT:
        if (cloudNetInterfaceAvailable())
        {
            ms_last_cloud_connected = millis();
        }
        else if (millis() - ms_last_cloud_connected > MESH_CLOUD_OFFLINE_TIMEOUT)
        {
            meshCloudInit(false);
            e_comms_state = MESH_COMMS_STATE_SYNC;
        }
        break;

    case MESH_COMMS_STATE_SYNC:
        result = meshCloudSync(b_force_sync);
        if (0 == result)
        {
            // in-progress
        }
        else if (result > 0)
        {
            // success
            b_force_sync  = false;
            e_comms_state = MESH_COMMS_STATE_CLOUD;
            ms_last_cloud_connected = millis();
        }
        else
        {
            // error
            e_comms_state = MESH_COMMS_STATE_INIT;
        }

        break;

    case MESH_COMMS_STATE_CLOUD:
        if (cloudNetInterfaceAvailable())
        {
            LOGI("connection was restored");
            b_force_sync  = true;
            e_comms_state = MESH_COMMS_STATE_INIT;
        }
        else if (millis() - meshCloudGetLastSync() > (3 * 60 * 60 * 1000UL))
        {
            LOGD("[re]check time");
            b_force_sync  = true;
            setLoraTimeSyncStatus(false);
            e_comms_state = MESH_COMMS_STATE_SYNC;
        }
        else if (!meshCloudCycle())
        {
            LOGW("failed cloud reports");
            e_comms_state = MESH_COMMS_STATE_SYNC;
        }
        else if (!meshCommandCycle())
        {
            LOGW("failed cloud command");
            e_comms_state = MESH_COMMS_STATE_SYNC;
        }
        else if (!meshUpdateCycle())
        {
            LOGW("failed cloud update");
            e_comms_state = MESH_COMMS_STATE_SYNC;
        }
        break;

    default:
        e_comms_state = MESH_COMMS_STATE_INIT;
        break;
    }
}

uint16_t meshCommsSend(uint32_t u32_dest_id, uint16_t u16_msg_id,
                        const uint8_t *pu8_data, uint8_t u8_length,
                        uint8_t u2_method, uint8_t u3_endpoint,
                        uint8_t u2_idx, uint8_t b_partial)
{
    mesh_node_st                    s_dst_node;
    mesh_data_msg_st                s_msg;
    mesh_comms_payload_st          *ps_payload  = (mesh_comms_payload_st *)s_msg.au8_data;
    mesh_comms_payload_header_st   *ps_hdr      = &ps_payload->s_hdr;
    uint16_t                        u16_len     = (uint16_t)u8_length;

    if (!getRoute(u32_dest_id, &s_dst_node))
    {
        if (0 == u16_msg_id)
        {
            LOGW("route to %08lx not found", u32_dest_id);
            return 0;
        }
        else // assume replying directly
        {
            LOGW("try reply to %08lx", u32_dest_id);
            s_dst_node.u32_first_hop = 0;
            s_dst_node.u32_id        = u32_dest_id;
        }
    }

    if (0 == u16_msg_id)
    {
        // generate new msg id
        if (0 == ++u16_message_counter)
        {
            u16_message_counter++; // skip 0 if integer overflow
        }
        u16_msg_id = u16_message_counter;
        LOGD("send %u bytes to %08lx", u8_length, u32_dest_id);
    }
    else // reply or resend
    {
        LOGD("reply %u bytes to %08lx", u8_length, u32_dest_id);
    }

    if ((NULL != pu8_data) && (u8_length > 0))
    {
        memcpy(ps_payload->au8_data, pu8_data, u8_length);
    }
    ps_hdr->u16_msg_id  = u16_msg_id;
    ps_hdr->u8_length   = u8_length;
    ps_hdr->u2_method   = u2_method;
    ps_hdr->u3_endpoint = u3_endpoint;
    ps_hdr->u2_idx      = u2_idx;
    ps_hdr->b_partial   = b_partial;

    memcpy(s_msg.s_hdr.mark, MESH_MSG_HEADER_MARK);
    s_msg.u32_orig          = meshNodeID();
    if (0 != s_dst_node.u32_first_hop)
    {
        s_msg.s_hdr.u32_dst = s_dst_node.u32_first_hop;
        s_msg.s_hdr.u32_src = s_dst_node.u32_id;
        s_msg.s_hdr.u8_type = MESH_MSG_FORWARD;
    }
    else
    {
        s_msg.s_hdr.u32_dst = s_dst_node.u32_id;
        s_msg.s_hdr.u32_src = meshNodeID();
        s_msg.s_hdr.u8_type = MESH_MSG_DIRECT;
    }

    // overheads
    u16_len += offsetof(mesh_comms_payload_st, au8_data);
    u16_len += offsetof(mesh_data_msg_st, au8_data);

    if (!meshSendRequest(&s_msg, u16_len))
    {
        ps_hdr->u16_msg_id = 0; // failed
    }

    return ps_hdr->u16_msg_id;
}


/*
 * Private Functions
 */

static void onLoraData(uint32_t fromID, uint8_t *rxPayload, uint16_t rxSize, int16_t rxRssi, int8_t rxSnr)
{
    //LOGD("Got data from node %08X: %s", fromID, rxPayload);
    mesh_comms_payload_st *ps_payload = (mesh_comms_payload_st *)rxPayload;
    
    switch (ps_payload->s_hdr.u2_method)
    {
    case MESH_COMMS_REQUEST_GET:
    case MESH_COMMS_REQUEST_PUT: // fall-through
        processClientRequest(fromID, ps_payload);
        break;
    case MESH_COMMS_RESPONSE:
        processServerResponse(fromID, ps_payload);
        break;
    default: // MESH_COMMS_LOCAL
        processLocalMessage(fromID, ps_payload);
        break;
    }
}

static void onNodesListChange(void)
{
    //LOGD("%s()", __func__);
    b_nodes_list_changed = true;
}

static void processClientRequest(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload)
{
    const mesh_comms_payload_header_st  *ps_hdr = &ps_payload->s_hdr;

    //LOGD("%s(%u, %.*s)", __func__, ps_hdr->u8_length, ps_hdr->u8_length, ps_payload->au8_data);

    switch (ps_hdr->u3_endpoint)
    {
    case MESH_COMMS_ENDPOINT_TIME: // get time
        meshCloudCurrentTime(u32_id_from, ps_payload);
        break;
    case MESH_COMMS_ENDPOINT_MONITOR:   // fall-through
    case MESH_COMMS_ENDPOINT_STATUS:    // fall-through
    case MESH_COMMS_ENDPOINT_EVENT:     // fall-through
    case MESH_COMMS_ENDPOINT_UPDATE:
        meshCloudHandleRequest(u32_id_from, ps_payload);
        break;
    default:
        break;
    }
}

static void processServerResponse(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload)
{
    UNUSED(u32_id_from);

    /* child node */
    meshCloudParseServerResponse(ps_payload);
    meshCommandParseServerResponse(ps_payload);
    meshUpdateParseServerResponse(ps_payload);
}

static void processLocalMessage(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload)
{
    //const mesh_comms_payload_header_st  *ps_hdr = &ps_payload->s_hdr;
    //LOGD("%s(%u, %.*s)", __func__, ps_hdr->u8_length, ps_hdr->u8_length, ps_payload->au8_data);

    /* parent node */
    meshCloudParseResponseAck(u32_id_from, ps_payload);

    /* child node */
    meshCloudParseSyncResponse(ps_payload);
    meshCloudParseReportAck(ps_payload);
    meshCommandParseRequestAck(ps_payload);
    meshUpdateParseRequestAck(ps_payload);
}
