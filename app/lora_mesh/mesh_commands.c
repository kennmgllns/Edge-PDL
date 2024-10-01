/*
 * mesh_cloud.c
 *
 */

#include <stdlib.h>

#include "global_defs.h"
#include "time_utils.h"
#include "system_flags.h"
#include "config_handler.h"

#include "edge_payload_cfg.h"
#include "edge_payload.h"

#include "input_monitoring.h"
#include "data_logging.h"

#include "modbus_bank_cfg.h"
#include "modbus_bank.h"

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
    enum {
        COMMAND_STATE_IDLE = 0,     // check if there's pending command
        COMMAND_STATE_GET_COMMAND,  // request for command details
        COMMAND_STATE_WAIT_COMMAND, // wait for command details from the cloud server
        COMMAND_STATE_PROCESS_CMD,  // execute the command
        COMMAND_STATE_SEND_RESULT,  // send process result
        COMMAND_STATE_WAIT_RESPONSE,// wait for 'put-status' ack from parent node
        COMMAND_STATE_RETRY_DELAY   // retry
    } e_state;
    struct {
        char        ac_key[24];     // command key
        char        ac_value[24];   // command value
        char        ac_hash[64+1];  // random sha256 hash (64 characters)
    } s_details;                    // parsed command details
    uint8_t         au8_payload_buf[MESH_COMMS_PAYLOAD_MAX_SIZE];
    size_t          sz_payload_len;
    uint16_t        u16_msg_id;
    uint32_t        ms_timeout;     // for response-wait & retry-delay
    uint16_t        u16_num_retries;
    bool            b_requested;    // command requested
    bool            b_status;       // command result
    bool            b_processed;    // response ready
    bool            b_mcureset;     // mcu reset required
    bool            b_ack_received;
    bool            b_ack_status;
} s_mesh_cmd;


/*
 * Private Function Prototypes
 */
static bool prepareGetCommandPayload(void);
static bool processCommand(void);
static bool preparePutResponsePayload(void);


/*
 * Public Functions
 */
void meshCommandInit(void)
{
    memset(&s_mesh_cmd, 0, sizeof(s_mesh_cmd));
}

bool meshCommandCycle(void)
{
    bool    b_status = true;

    switch (s_mesh_cmd.e_state)
    {
    case COMMAND_STATE_IDLE:
        if (false == s_mesh_cmd.b_requested)
        {
            // no pending command
        }
        else if (0 == meshCloudGetParentId())
        {
            // no parent yet, needs sync
            LOGW("no parent node");
            b_status = false;
        }
        else if (prepareGetCommandPayload())
        {
            memset(&s_mesh_cmd.s_details, 0, sizeof(s_mesh_cmd.s_details));
            s_mesh_cmd.e_state = COMMAND_STATE_GET_COMMAND;
        }
        break;

    case COMMAND_STATE_GET_COMMAND:
        s_mesh_cmd.b_ack_received = false;
        s_mesh_cmd.b_ack_status   = false;
        s_mesh_cmd.ms_timeout     = millis();
        s_mesh_cmd.u16_msg_id     = meshCommsSend(meshCloudGetParentId(), 0, s_mesh_cmd.au8_payload_buf, s_mesh_cmd.sz_payload_len,
                                                    MESH_COMMS_REQUEST_GET, MESH_COMMS_ENDPOINT_STATUS, 0, 0);
        LOGD("send get-request %u", s_mesh_cmd.u16_msg_id);
        if (s_mesh_cmd.u16_msg_id > 0)
        {
            memset(s_mesh_cmd.au8_payload_buf, 0, sizeof(s_mesh_cmd.au8_payload_buf));
            s_mesh_cmd.sz_payload_len = 0;
            s_mesh_cmd.e_state        = COMMAND_STATE_WAIT_COMMAND;
        }
        else
        {
            s_mesh_cmd.e_state = COMMAND_STATE_RETRY_DELAY;
        }
        break;

    case COMMAND_STATE_WAIT_COMMAND:
        if (s_mesh_cmd.sz_payload_len > 0)
        {
            s_mesh_cmd.b_requested      = false;
            s_mesh_cmd.u16_num_retries  = 0;
            s_mesh_cmd.e_state          = COMMAND_STATE_PROCESS_CMD;
        }
        else if (millis() - s_mesh_cmd.ms_timeout > K_MESH_CLOUD_REQUEST_RESPONSE_TIMEOUT)
        {
            LOGW("response timeout");
            s_mesh_cmd.ms_timeout = millis();
            s_mesh_cmd.e_state    = COMMAND_STATE_RETRY_DELAY;
        }
        break;

    case COMMAND_STATE_PROCESS_CMD:
        s_mesh_cmd.b_status    = processCommand();
        s_mesh_cmd.b_processed = preparePutResponsePayload();
        if (true == s_mesh_cmd.b_processed)
        {
            s_mesh_cmd.u16_msg_id = 0;
            s_mesh_cmd.e_state    = COMMAND_STATE_SEND_RESULT;
        }
        else // should not go here
        {
            s_mesh_cmd.e_state = COMMAND_STATE_RETRY_DELAY;
            b_status           = false;
        }
        break;

    case COMMAND_STATE_SEND_RESULT:
        s_mesh_cmd.b_ack_received = false;
        s_mesh_cmd.b_ack_status   = false;
        s_mesh_cmd.ms_timeout     = millis();
        s_mesh_cmd.u16_msg_id     = meshCommsSend(meshCloudGetParentId(), s_mesh_cmd.u16_msg_id, s_mesh_cmd.au8_payload_buf, s_mesh_cmd.sz_payload_len,
                                                    MESH_COMMS_REQUEST_PUT, MESH_COMMS_ENDPOINT_STATUS, 0, 0);
        LOGD("send put-request %u", s_mesh_cmd.u16_msg_id);
        if (s_mesh_cmd.u16_msg_id > 0)
        {
            s_mesh_cmd.e_state = COMMAND_STATE_WAIT_RESPONSE;
        }
        else
        {
            s_mesh_cmd.e_state = COMMAND_STATE_RETRY_DELAY;
        }
        break;

    case COMMAND_STATE_WAIT_RESPONSE:
        if (true == s_mesh_cmd.b_ack_received)
        {
            if (true == s_mesh_cmd.b_ack_status)
            {
                LOGI("command result %u (retry %u)", s_mesh_cmd.b_status, s_mesh_cmd.u16_num_retries);
                if (s_mesh_cmd.b_mcureset)
                {
                    (void)setMcuResetRequestStatus(TRUE, SYS_RESET_CLOUD_COMMAND);
                }
                s_mesh_cmd.e_state = COMMAND_STATE_IDLE;
            }
            else // nack, e.g. queue full
            {
                LOGW("response error");
                s_mesh_cmd.e_state = COMMAND_STATE_RETRY_DELAY;
            }
        }
        else if (millis() - s_mesh_cmd.ms_timeout > K_MESH_CLOUD_REPORT_ACK_TIMEOUT)
        {
            LOGW("ACK timeout");
            s_mesh_cmd.e_state = COMMAND_STATE_RETRY_DELAY;
        }

        break;

    case COMMAND_STATE_RETRY_DELAY:
        if (true == s_mesh_cmd.b_processed)
        {
            s_mesh_cmd.u16_num_retries += 1;
            // if still within 3 mminutes (180 seconds) ...
            if (s_mesh_cmd.u16_num_retries < (180 / 5 /*ack timeout*/))
            {
                // just resend the result
                s_mesh_cmd.e_state = COMMAND_STATE_SEND_RESULT;
            }
            else
            {
                meshCommandInit();
                b_status = false;
            }
        }
        else
        {
            // nothing to do here
            // the server itself will just re-queue the command
            s_mesh_cmd.e_state = COMMAND_STATE_IDLE;
        }
        break;
    
    default:
        s_mesh_cmd.e_state = COMMAND_STATE_IDLE;
        break;
    }

    return b_status;
}

bool meshCommandParseRequestAck(const mesh_comms_payload_st *ps_payload)
{
    if (ps_payload->s_hdr.u16_msg_id == s_mesh_cmd.u16_msg_id)
    {
        s_mesh_cmd.b_ack_received = true;
        s_mesh_cmd.b_ack_status   = (0 == memcmp(ps_payload->au8_data, "ACK", 3));
    }
    return s_mesh_cmd.b_ack_status;
}

void meshCommandParseServerResponse(const mesh_comms_payload_st *ps_payload)
{
    const mesh_comms_payload_header_st  *ps_hdr = &ps_payload->s_hdr;

    if (1 == ps_hdr->u8_length) // if a response byte
    {
        if ((EP_SERVER_RESP_OPERATION_REQ == ps_payload->au8_data[0]) &&
            (COMMAND_STATE_IDLE == s_mesh_cmd.e_state))
        {
            s_mesh_cmd.b_requested = true; // command request queued
        }
    }
    else if (ps_hdr->u16_msg_id != s_mesh_cmd.u16_msg_id)
    {
        // not for us
    }
    else // got command details
    {
        s_mesh_cmd.u16_msg_id     = 0; // done
        s_mesh_cmd.sz_payload_len = ps_hdr->u8_length;
        if (s_mesh_cmd.sz_payload_len > sizeof(s_mesh_cmd.au8_payload_buf))
        {
            s_mesh_cmd.sz_payload_len = sizeof(s_mesh_cmd.au8_payload_buf);
        }
        memcpy(s_mesh_cmd.au8_payload_buf, ps_payload->au8_data, s_mesh_cmd.sz_payload_len);
    }
}

/*
 * Private Functions
 */
static bool prepareGetCommandPayload(void)
{
    int32_t                 si32_timestamp;
    ep_status_payload_st    s_status_payload;

    memset(&s_status_payload, 0, sizeof(s_status_payload));

    if (false == timeUtilGetRtcNowEpoch(&si32_timestamp))
    {
        //
    }
    else if (false == edgePayloadInitComHeader(&s_status_payload.s_header, si32_timestamp))
    {
        //
    }
    else if (true == edgePayloadComHeader2Buf(s_mesh_cmd.au8_payload_buf, sizeof(s_mesh_cmd.au8_payload_buf),
                                                &s_mesh_cmd.sz_payload_len, &s_status_payload.s_header))
    {
        s_mesh_cmd.au8_payload_buf[s_mesh_cmd.sz_payload_len++] = EP_SERVER_RESP_OPERATION_REQ;
        //LOGD("payload len=%lu", s_mesh_cmd.sz_payload_len);
        return true;
    }

    //LOGW("%s failed", __func__);
    return false;
}

static bool processCommand(void)
{
    bool    b_status        = false;
    bool    b_event_change  = false;
    
    memset(&s_mesh_cmd.s_details, 0, sizeof(s_mesh_cmd.s_details));
    // simple json parse: {"key":"value","hash":"hash_value"}
    if (3 != sscanf((char *)s_mesh_cmd.au8_payload_buf, "{\"%23[^\"]\":\"%23[^\"]\",\"hash\":\"%64[^\"]\"}",
                    s_mesh_cmd.s_details.ac_key, s_mesh_cmd.s_details.ac_value, s_mesh_cmd.s_details.ac_hash))
    {
        LOGW("invalid command: %.*s", s_mesh_cmd.sz_payload_len, s_mesh_cmd.au8_payload_buf);
    }
#if 0 // ignore
    else if (64 != strlen(s_mesh_cmd.s_details.ac_hash))
    {
        LOGW("invalid hash");
    }
#endif
    else
    {
        LOGD("command: %s=%s (hash=%s)", s_mesh_cmd.s_details.ac_key, s_mesh_cmd.s_details.ac_value, s_mesh_cmd.s_details.ac_hash);

      #define CMD_IS(cmd)        (0 == strncmp(s_mesh_cmd.s_details.ac_key, cmd, __builtin_strlen(cmd)))

        if (CMD_IS("mcureset"))
        {
            LOGI("mcu reset");
            s_mesh_cmd.b_mcureset = true;
            b_status = true;
        }
        else if (CMD_IS("restorefactory"))
        {
            LOGI("restore factory");
            (void)setFactoryResetRequestStatus(TRUE);
            resetUserConfig();
            s_mesh_cmd.b_mcureset = true;
            b_status = true;
        }
        else if (CMD_IS("transformerconfig"))
        {
            int phase_config = atoi(s_mesh_cmd.s_details.ac_value);
            if ((IN_MON_1PHASE_2WIRE <= phase_config) && (IN_MON_3PHASE_3WIRE_DELTA_MID >= phase_config))
            {
                setPhaseConfig((in_phase_config_et)phase_config);
                configHandlerSetPhaseConfig(FALSE, phase_config);
                (void)modbusBankSetHoldingRegisterValue(MODBUS_IDX_HR_PHASE_CONFIG, (uint16_t)(phase_config), TRUE);
                LOGI("transformer config set to %d", phase_config);
                b_event_change  = true;
                b_status        = true;
            }
            else
            {
                LOGW("transformer config=%d error", phase_config);
            }
        }
        else if (CMD_IS("statuslogs"))
        {
            int statuslogs = atoi(s_mesh_cmd.s_details.ac_value);
            setStatusLoggingSend((uint8_t)statuslogs);
            configHandlerSetLoggingSend(FALSE, STATUS_LOGGING_SEND, (uint8_t)statuslogs);
            LOGI("status log - %u", statuslogs);
            b_event_change  = true;
            b_status        = true;
        }
        else
        {
            // TO DO: implement all commands from "Core\Src\app\cloud_comms\cloud_commands.c"

            LOGW("not supported command: %s=%s", s_mesh_cmd.s_details.ac_key, s_mesh_cmd.s_details.ac_value);
        }

        if (true == b_event_change)
        {
            configHandlerSync();
            LOGD("Save changes to emmc..");
        }
    }
    
    return b_status;
}

static bool preparePutResponsePayload(void)
{
    ep_status_payload_st    s_status_payload;
    int32_t                 si32_timestamp;

    char   *pc_json_buf = (char *)&s_mesh_cmd.s_details; // reuse buffer
    size_t  sz_json_len = snprintf(pc_json_buf, sizeof(s_mesh_cmd.s_details), "{\"status\":\"%s\",\"hash\":\"%.64s\"}",
                                    s_mesh_cmd.b_status ? "ok" : "not ok", s_mesh_cmd.s_details.ac_hash);

    memset(&s_status_payload, 0, sizeof(s_status_payload));
    if (false == timeUtilGetRtcNowEpoch(&si32_timestamp))
    {
        //
    }
    else if (false == edgePayloadInitComHeader(&s_status_payload.s_header, si32_timestamp))
    {
        //
    }
    else if (false == edgePayloadAddStatusTag(&s_status_payload, EP_STATUS_TAG_OPER_CMD_PROCESSED,
                                                (uint8_t *)pc_json_buf, (uint8_t)(sz_json_len + 1))) // include trailing null termination
    {
        /* error on adding status tag */
    }
    else if (true == edgePayloadStatus2Buf(s_mesh_cmd.au8_payload_buf, sizeof(s_mesh_cmd.au8_payload_buf), &s_mesh_cmd.sz_payload_len, &s_status_payload))
    {
        LOGD("%.*s", sz_json_len, pc_json_buf);
        return true;
    }

    LOGD("prepare status response failed");
    return false;
}
