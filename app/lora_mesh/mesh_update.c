
/*
 * mesh_update.c
 *
 */

#include "global_defs.h"
#include "system_flags.h"
#include "time_utils.h"

#include "crc16.h"
#include "hash_sha256.h"

#include <ff.h> // emmc fatfs
#include "nvmem.h"

#include "edge_payload_cfg.h"
#include "edge_payload.h"

#include "cloud_update_cfg.h"

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
        UPDATE_STATE_IDLE = 0,      // check if there's pending update request
        UPDATE_STATE_GET_INFO,      // request for update file details
        UPDATE_STATE_WAIT_INFO,     // wait for update file details from the cloud server
        UPDATE_STATE_DOWNLOAD_FILE, // download update file by chunks
        UPDATE_STATE_SEND_RESULT,   // send fw-update result
        UPDATE_STATE_WAIT_RESPONSE, // wait for 'put-status' ack from parent node
        UPDATE_STATE_RETRY_DELAY    // retry
    } e_state;
    enum {
        UPDATE_RESULT_UNKNOWN = 0,          // initial|on-going download
        UPDATE_RESULT_INTERNAL_ERROR,       // emmc errors, etc.
        UPDATE_RESULT_UPDATE_INFO_ERROR,    // parse info failed
        UPDATE_RESULT_DOWNLOAD_ERROR,       // download failed
        UPDATE_RESULT_HASH_MISMATCH_ERROR,  // wrong hash
        UPDATE_RESULT_UPDATE_STARTED        // success
    } e_result;
    struct {
        char        ac_name[128];   // filename from cloud
        char        ac_hash[64+1];  // sha256 hash string
        char        ac_user[15+1];  // [not used] IPv4 address of the one initiated the fw update
        uint32_t    ui32_size;      // total file size (few hundred kb's)
    } s_details;
    uint8_t         au8_payload_buf[MESH_COMMS_PAYLOAD_MAX_SIZE];
    size_t          sz_payload_len;
    uint16_t        u16_msg_id;
    uint32_t        ms_timeout;     // for response-wait & retry-delay
    uint16_t        u16_num_retries;
    bool            b_requested;    // update requested
    bool            b_processed;    // response ready
    bool            b_ack_received;
    bool            b_ack_status;
} s_mesh_update;

static struct {
    enum {
        DOWNLOAD_STATE_IDLE = 0,        // prepare request
        DOWNLOAD_STATE_GET_CHUNK,       // send 'get /ud' request to parent node
        DOWNLOAD_STATE_WAIT_CHUNK,      // wait file chunk forwarded by parent node
        DOWNLOAD_STATE_VERIFY,          // check downloaded file
        DOWNLOAD_STATE_RETRY_DELAY      // retry
    } e_state;
    FIL             s_file;             // emmc file object
    uint32_t        ui32_chunk_offset;  // read file chunk offset
    uint16_t        ui16_chunk_len;     // read file chunk length
    uint16_t        ui16_chunk_err;     // chunk errors counter
} s_download; // file download substate

/*
 * Private Function Prototypes
 */
static bool prepareGetUpdateInfoPayload(void);
static bool prepareGetUpdateFilePayload(void);
static bool preparePutUpdateInfoPayload(void);

static bool parseFileInfo(void);
static bool parseFileChunks(void);
static bool verifyFileDownload(void);

static bool processDownload(void);


/*
 * Public Functions
 */
void meshUpdateInit(void)
{
    memset(&s_mesh_update, 0, sizeof(s_mesh_update));
    memset(&s_download, 0, sizeof(s_download));
}

bool meshUpdateCycle(void)
{
    bool b_status = true;

    switch (s_mesh_update.e_state)
    {
    case UPDATE_STATE_IDLE:
        if (false == s_mesh_update.b_requested)
        {
            // no pending fw update
        }
        else if (0 == meshCloudGetParentId())
        {
            // no parent yet, needs sync
            //LOGW("no parent node");
            b_status = false;
        }
        else if (prepareGetUpdateInfoPayload())
        {
            memset(&s_mesh_update.s_details, 0, sizeof(s_mesh_update.s_details));
            s_mesh_update.e_state = UPDATE_STATE_GET_INFO;
        }
        else
        {
            // payload error
            b_status = false;
        }
        break;

    case UPDATE_STATE_GET_INFO:
        s_mesh_update.ms_timeout = millis();
        s_mesh_update.u16_msg_id = meshCommsSend(meshCloudGetParentId(), 0,
                                                s_mesh_update.au8_payload_buf, s_mesh_update.sz_payload_len,
                                                MESH_COMMS_REQUEST_GET, MESH_COMMS_ENDPOINT_STATUS, 0, 0);
        LOGD("send get-request %u", s_mesh_update.u16_msg_id);
        if (s_mesh_update.u16_msg_id > 0)
        {
            memset(s_mesh_update.au8_payload_buf, 0, sizeof(s_mesh_update.au8_payload_buf));
            s_mesh_update.sz_payload_len = 0;
            s_mesh_update.e_state        = UPDATE_STATE_WAIT_INFO;
        }
        else
        {
            s_mesh_update.e_state = UPDATE_STATE_RETRY_DELAY;
        }
        break;

    case UPDATE_STATE_WAIT_INFO:
        if (s_mesh_update.sz_payload_len > 0)
        {
            s_mesh_update.b_requested = false;
            if (true == parseFileInfo())
            {
                s_mesh_update.e_state = UPDATE_STATE_DOWNLOAD_FILE; // proceed to download sub-state
            }
            else
            {
                s_mesh_update.e_state = UPDATE_STATE_SEND_RESULT; // send error result
            }
        }
        else if (millis() - s_mesh_update.ms_timeout > K_MESH_CLOUD_REQUEST_RESPONSE_TIMEOUT)
        {
            LOGW("response timeout");
            s_mesh_update.ms_timeout = millis();
            s_mesh_update.e_state    = UPDATE_STATE_RETRY_DELAY;
        }
        break;
    
    case UPDATE_STATE_DOWNLOAD_FILE:
        if (true == processDownload())
        {
            if (UPDATE_RESULT_UNKNOWN == s_mesh_update.e_result)
            {
                // still ongoing download
            }
            else if (true == (s_mesh_update.b_processed = preparePutUpdateInfoPayload()))
            {
                s_mesh_update.u16_msg_id = 0;
                s_mesh_update.e_state    = UPDATE_STATE_SEND_RESULT;
            }
            else // should not go here
            {
                b_status = false;
            }
        }
        else
        {
            s_mesh_update.e_state = UPDATE_STATE_RETRY_DELAY;
        }
        break;

    case UPDATE_STATE_SEND_RESULT:
        s_mesh_update.b_ack_received = false;
        s_mesh_update.b_ack_status   = false;
        s_mesh_update.ms_timeout     = millis();
        s_mesh_update.u16_msg_id     = meshCommsSend(meshCloudGetParentId(), s_mesh_update.u16_msg_id,
                                                    s_mesh_update.au8_payload_buf, s_mesh_update.sz_payload_len,
                                                    MESH_COMMS_REQUEST_PUT, MESH_COMMS_ENDPOINT_STATUS, 0, 0);
        LOGD("send put-request %u", s_mesh_update.u16_msg_id);
        if (s_mesh_update.u16_msg_id > 0)
        {
            s_mesh_update.e_state = UPDATE_STATE_WAIT_RESPONSE;
        }
        else
        {
            s_mesh_update.e_state = UPDATE_STATE_RETRY_DELAY;
        }
        break;
    
    case UPDATE_STATE_WAIT_RESPONSE:
        if (true == s_mesh_update.b_ack_received)
        {
            if (true == s_mesh_update.b_ack_status)
            {
                s_mesh_update.e_state = UPDATE_STATE_IDLE;
                if (UPDATE_RESULT_UPDATE_STARTED == s_mesh_update.e_result)
                {
                    LOGI("reboot to update");
                    setBootflagStatus(BOOTFLAG_UPDATE_APP);
                    (void)setMcuResetRequestStatus(TRUE, SYS_RESET_BOOTLOAD);
                }
            }
            else // nack, e.g. queue full
            {
                LOGW("response error");
                s_mesh_update.e_state = UPDATE_STATE_RETRY_DELAY;
            }
        }
        else if (millis() - s_mesh_update.ms_timeout > K_MESH_CLOUD_REPORT_ACK_TIMEOUT)
        {
            LOGW("ACK timeout");
            s_mesh_update.e_state = UPDATE_STATE_RETRY_DELAY;
        }
        break;
    
    case UPDATE_STATE_RETRY_DELAY:
        if (true == s_mesh_update.b_processed)
        {
            s_mesh_update.u16_num_retries += 1;
            // if still within 10 minutes (600 seconds) ...
            if (s_mesh_update.u16_num_retries < (600 / 5 /*ack timeout*/))
            {
                // just resend the result
                s_mesh_update.e_state = UPDATE_STATE_SEND_RESULT;
            }
            else
            {
                b_status = false;
            }
        }
        else
        {
            // nothing to do here
            // the server itself will just re-queue the update request
            s_mesh_update.e_state = UPDATE_STATE_IDLE;
        }
        break;

    default:
        s_mesh_update.e_state = UPDATE_STATE_IDLE;
        break;
    }

    return b_status;
}

bool meshUpdateParseRequestAck(const mesh_comms_payload_st *ps_payload)
{
    if (ps_payload->s_hdr.u16_msg_id == s_mesh_update.u16_msg_id)
    {
        s_mesh_update.b_ack_received = true;
        s_mesh_update.b_ack_status   = (0 == memcmp(ps_payload->au8_data, "ACK", 3));
    }
    return s_mesh_update.b_ack_status;
}

void meshUpdateParseServerResponse(const mesh_comms_payload_st *ps_payload)
{
    const mesh_comms_payload_header_st  *ps_hdr = &ps_payload->s_hdr;

    if (1 == ps_hdr->u8_length) // if a response byte
    {
        if ((EP_SERVER_RESP_UPDATE_REQ == ps_payload->au8_data[0]) &&
            (UPDATE_STATE_IDLE == s_mesh_update.e_state))
        {
            LOGD("update request");
            s_mesh_update.b_requested     = true; // update request queued
            s_mesh_update.u16_num_retries = 0;
        }
    }
    else if (ps_hdr->u16_msg_id != s_mesh_update.u16_msg_id)
    {
        // not for us
    }
    else // got either update details or file chunk/contents
    {
        s_mesh_update.u16_msg_id     = 0; // done
        s_mesh_update.sz_payload_len = ps_hdr->u8_length;
        if (s_mesh_update.sz_payload_len > sizeof(s_mesh_update.au8_payload_buf))
        {
            s_mesh_update.sz_payload_len = sizeof(s_mesh_update.au8_payload_buf);
        }
        memcpy(s_mesh_update.au8_payload_buf, ps_payload->au8_data, s_mesh_update.sz_payload_len);
    }
}

/*
 * Private Function Prototypes
 */
static bool prepareGetUpdateInfoPayload(void)
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
    else if (true == edgePayloadComHeader2Buf(s_mesh_update.au8_payload_buf, sizeof(s_mesh_update.au8_payload_buf),
                                                &s_mesh_update.sz_payload_len, &s_status_payload.s_header))
    {
        s_mesh_update.au8_payload_buf[s_mesh_update.sz_payload_len++] = EP_SERVER_RESP_UPDATE_REQ;
        //LOGD("payload len=%lu", s_mesh_update.sz_payload_len);
        return true;
    }

    return false;
}

static bool prepareGetUpdateFilePayload(void)
{
#if 0 // to do: need to modify coap-dtls-client library to handle larger packets
    static const uint16_t       U16_MAX_CHUNK_SIZE = MESH_COMMS_PAYLOAD_MAX_SIZE - 8 /*8-byte chunk header*/ - 1;
#else
    static const uint16_t       U16_MAX_CHUNK_SIZE = K_CLOUD_UPDATE_CHUNK_SIZE;
#endif
    ep_update_get_payload_st    s_update_payload;
    int32_t                     si32_timestamp;
    uint32_t                    ui32_remaining;

    ui32_remaining = s_mesh_update.s_details.ui32_size - s_download.ui32_chunk_offset;
    if (ui32_remaining >= (uint32_t)U16_MAX_CHUNK_SIZE)
    {
        s_download.ui16_chunk_len = U16_MAX_CHUNK_SIZE;
    }
    else
    {
        s_download.ui16_chunk_len = (uint16_t)ui32_remaining;
    }

    if (false == timeUtilGetRtcNowEpoch(&si32_timestamp))
    {
        //
    }
    else if (false == edgePayloadInitComHeader(&s_update_payload.s_header, si32_timestamp))
    {
        //
    }
    else if (false == edgePayloadSetUpdateGetContent(&s_update_payload, s_download.ui32_chunk_offset, s_download.ui16_chunk_len))
    {
        //
    }
    else if (true == edgePayloadUpdateGet2Buf(s_mesh_update.au8_payload_buf, sizeof(s_mesh_update.au8_payload_buf), &s_mesh_update.sz_payload_len, &s_update_payload))
    {
        return true;
    }

    return false;
}

static bool preparePutUpdateInfoPayload(void)
{
    ep_status_payload_st    s_status_payload;
    const char             *pc_result = NULL;
    int32_t                 si32_timestamp;

    switch (s_mesh_update.e_result)
    {
        case UPDATE_RESULT_UPDATE_INFO_ERROR:   pc_result = "info error";     break;
        case UPDATE_RESULT_DOWNLOAD_ERROR:      pc_result = "download error"; break;
        case UPDATE_RESULT_HASH_MISMATCH_ERROR: pc_result = "hash error";     break;
        case UPDATE_RESULT_UPDATE_STARTED:      pc_result = "update started"; break; // success
        default:                                pc_result = "internal error"; break;
    }

    char   *pc_json_buf = (char *)&s_mesh_update.s_details; // reuse buffer
    size_t  sz_json_len = snprintf(pc_json_buf, sizeof(s_mesh_update.s_details), "{\"%s\":\"%s\",\"hash\":\"%.64s\"}",
                                    (UPDATE_RESULT_UPDATE_STARTED == s_mesh_update.e_result) ? "done" : "error",
                                    pc_result, s_mesh_update.s_details.ac_hash);

    memset(&s_status_payload, 0, sizeof(s_status_payload));
    if (false == timeUtilGetRtcNowEpoch(&si32_timestamp))
    {
        //
    }
    else if (false == edgePayloadInitComHeader(&s_status_payload.s_header, si32_timestamp))
    {
        //
    }
    else if (false == edgePayloadAddStatusTag(&s_status_payload, EP_STATUS_TAG_UPDATE_INFO,
                                                (uint8_t *)pc_json_buf, (uint8_t)(sz_json_len + 1))) // include trailing null termination
    {
        /* error on adding status tag */
    }
    else if (true == edgePayloadStatus2Buf(s_mesh_update.au8_payload_buf, sizeof(s_mesh_update.au8_payload_buf), &s_mesh_update.sz_payload_len, &s_status_payload))
    {
        LOGD("%.*s", sz_json_len, pc_json_buf);
        return true;
    }

    LOGD("prepare status response failed");
    return false;
}

static bool parseFileInfo(void)
{
    FRESULT e_res;
    char   *pc_ch;
    
    memset(&s_mesh_update.s_details, 0, sizeof(s_mesh_update.s_details));
    s_mesh_update.e_result = UPDATE_RESULT_UPDATE_INFO_ERROR;

    if ( // simple json parse: {"file":"name","hash":"hash_value","user":"ipv4","size":size}
        (4 == sscanf((char *)s_mesh_update.au8_payload_buf, "{\"file\":\"%127[^\"]\",\"hash\":\"%64[^\"]\",\"user\":\"%15[^\"]\",\"size\":%lu}",
                    s_mesh_update.s_details.ac_name, s_mesh_update.s_details.ac_hash, s_mesh_update.s_details.ac_user, &s_mesh_update.s_details.ui32_size))
        || // or json parse: {"file":"name","hash":"hash_value","size":size}
        (3 == sscanf((char *)s_mesh_update.au8_payload_buf, "{\"file\":\"%127[^\"]\",\"hash\":\"%64[^\"]\",\"size\":%lu}",
                    s_mesh_update.s_details.ac_name, s_mesh_update.s_details.ac_hash, &s_mesh_update.s_details.ui32_size)) )
    {
        if (NULL != (pc_ch = strrchr(s_mesh_update.s_details.ac_name, '/')))
        { // get filename only
            memmove(s_mesh_update.s_details.ac_name, pc_ch + 1, strlen(pc_ch + 1) + 1);
        }

        (void)f_close(&s_download.s_file); // prevent multiple access to the emmc file

        if ((NULL == (pc_ch = strrchr(s_mesh_update.s_details.ac_name, '.'))) ||
            (0 != strcmp(pc_ch, ".bin"))) // ARM bin file only
        {
            LOGW("not supported file %s", s_mesh_update.s_details.ac_name);
        }
        else if (64 != strlen(s_mesh_update.s_details.ac_hash))
        {
            LOGW("not supported hash: %s", s_mesh_update.s_details.ac_hash);
        }
        else if ((s_mesh_update.s_details.ui32_size < (32 * 1024)) || (s_mesh_update.s_details.ui32_size > (512 * 1024)))
        {
            LOGW("not supported size: %lu", s_mesh_update.s_details.ui32_size);
        }
        else if (FR_OK != (e_res = f_open(&s_download.s_file, K_MESH_DOWNLOAD_TMP_FILENAME, FA_CREATE_ALWAYS | FA_WRITE | FA_READ)))
        {
            LOGW("failed to create local file (error=%d)", e_res);
            s_mesh_update.e_result = UPDATE_RESULT_INTERNAL_ERROR;
        }
        else
        {
            LOGI("file: %s (%lukB)", s_mesh_update.s_details.ac_name, s_mesh_update.s_details.ui32_size >> 10);
            LOGI("hash: %s", s_mesh_update.s_details.ac_hash);

            s_mesh_update.e_result          = UPDATE_RESULT_UNKNOWN; // on-going download
            s_download.e_state              = DOWNLOAD_STATE_IDLE;
            s_download.ui32_chunk_offset    = 0;
            s_download.ui16_chunk_len       = 0;
            s_download.ui16_chunk_err       = 0;
            return true;
        }
    }
    else
    {
        LOGW("invalid info: %.*s", s_mesh_update.sz_payload_len, s_mesh_update.au8_payload_buf);
    }

    return false;
}

static bool parseFileChunks(void)
{
    struct {
        uint32_t    ui32_offset;
        uint16_t    ui16_len;
        uint16_t    ui16_crc;
    } s_chunk; // file chunk header
    FRESULT         e_res;         // file access result
    unsigned        u_bw;          // number of bytes writen to the file
    uint8_t        *pui8_buff;
    uint16_t        ui16_calc_crc; // expected crc16 value of the chunk

    pui8_buff = s_mesh_update.au8_payload_buf;
    // parse chunk header
    memcpy(&s_chunk, pui8_buff, sizeof(s_chunk));
    //LOGD("%s: off %lu len %u crc %04x", __func__, s_chunk.ui32_offset, s_chunk.ui16_len, s_chunk.ui16_crc);
    pui8_buff += sizeof(s_chunk);

    if (s_mesh_update.sz_payload_len != (sizeof(s_chunk) + s_chunk.ui16_len))
    {
        LOGW("wrong payload length (%lu != %lu)", s_mesh_update.sz_payload_len, sizeof(s_chunk) + s_chunk.ui16_len);
    }
    else if (s_download.ui32_chunk_offset != s_chunk.ui32_offset)
    {
        //LOGW("wrong chunk offset (%lu != %lu)", s_download.ui32_chunk_offset, s_chunk.ui32_offset);
    }
    else if (s_download.ui16_chunk_len != s_chunk.ui16_len)
    {
        //LOGW("wrong chunk length (%u != %u)", s_download.ui16_chunk_len, s_chunk.ui16_len);
    }
    else if (s_chunk.ui16_crc != (ui16_calc_crc = crc16CalcBlock(pui8_buff, s_chunk.ui16_len)))
    {
        LOGW("wrong chunk crc (%04x != %04x)", ui16_calc_crc, s_chunk.ui16_crc);
    }
    else if ((FR_OK != (e_res = f_write(&s_download.s_file, pui8_buff, s_chunk.ui16_len, &u_bw))) ||
             (s_chunk.ui16_len != u_bw))
    {
        LOGW("write (%u/%u) failed (error=%d)", u_bw, s_chunk.ui16_len, e_res);
        s_mesh_update.e_result = UPDATE_RESULT_INTERNAL_ERROR;
    }
    else // success
    {
        // increment offset
        s_download.ui32_chunk_offset += s_chunk.ui16_len;
        LOGI("%s : %lu%% (%lu/%lu)", s_mesh_update.s_details.ac_name,
            (s_download.ui32_chunk_offset * 100UL) / s_mesh_update.s_details.ui32_size,
             s_download.ui32_chunk_offset, s_mesh_update.s_details.ui32_size);

        // reset error counter
        s_download.ui16_chunk_err = 0;
        return true;
    }

    return false;
}

static bool verifyFileDownload(void)
{
    sha256_context_st   s_sha256_ctx;
    uint32_t            ui32_remaining;
    unsigned            bytes_to_read, bytes_read; // file read counter
    bool                b_result = false;

    if (FR_OK != f_lseek(&s_download.s_file, 0))
    {
        LOGW("failed to rewind");
    }
    else
    {
        sha256Init(&s_sha256_ctx);
        ui32_remaining = s_mesh_update.s_details.ui32_size;
        while (ui32_remaining > 0)
        {
            bytes_to_read = ui32_remaining;
            if (bytes_to_read > sizeof(s_mesh_update.au8_payload_buf)) {
                bytes_to_read = sizeof(s_mesh_update.au8_payload_buf);
            }
            if ((FR_OK != f_read(&s_download.s_file, s_mesh_update.au8_payload_buf, bytes_to_read, &bytes_read)) ||
                (bytes_read != bytes_to_read)) { // re-use 'au8_payload_buf' for hash input
                LOGW("read (%u/%u) failed", bytes_read, bytes_to_read);
                break;
            }
            sha256Update(&s_sha256_ctx, s_mesh_update.au8_payload_buf, bytes_read);
            ui32_remaining -= bytes_read;
        }
        sha256Finalize(&s_sha256_ctx, s_mesh_update.au8_payload_buf);
        char *pc_str = s_mesh_update.s_details.ac_name; // re-use 'ac_name' for hash output
        for (uint8_t ui8_idx = 0; ui8_idx < 32; ui8_idx++) {
            pc_str += snprintf(pc_str, 3, "%02x", s_mesh_update.au8_payload_buf[ui8_idx]);
        }

        b_result = (0 == strncmp(s_mesh_update.s_details.ac_name, s_mesh_update.s_details.ac_hash, sizeof(s_mesh_update.s_details.ac_hash)));
    }

    return b_result;
}

static bool processDownload(void)
{
    bool b_status = true;

    switch (s_download.e_state)
    {
    case DOWNLOAD_STATE_IDLE:
        if (s_download.ui32_chunk_offset >= s_mesh_update.s_details.ui32_size)
        {
            // download already done
        }
        else if (true == prepareGetUpdateFilePayload())
        {
            s_download.e_state = DOWNLOAD_STATE_GET_CHUNK;
        }
        else
        {
            b_status = false; // payload error
        }
        break;

    case DOWNLOAD_STATE_GET_CHUNK:
        s_mesh_update.ms_timeout = millis();
        s_mesh_update.u16_msg_id = meshCommsSend(meshCloudGetParentId(), 0,
                                                s_mesh_update.au8_payload_buf, s_mesh_update.sz_payload_len,
                                                MESH_COMMS_REQUEST_GET, MESH_COMMS_ENDPOINT_UPDATE, 0, 0);
        //LOGD("send get-request %u", s_mesh_update.u16_msg_id);
        if (s_mesh_update.u16_msg_id > 0)
        {
            memset(s_mesh_update.au8_payload_buf, 0, sizeof(s_mesh_update.au8_payload_buf));
            s_mesh_update.sz_payload_len = 0;
            s_download.e_state           = DOWNLOAD_STATE_WAIT_CHUNK;
        }
        else
        {
            s_download.e_state = DOWNLOAD_STATE_RETRY_DELAY;
        }
        break;

    case DOWNLOAD_STATE_WAIT_CHUNK:
        if (s_mesh_update.sz_payload_len > 0)
        {
            if (true == parseFileChunks())
            {
        #if 0 // test: few chunks only (skip download)
                if (s_download.ui32_chunk_offset >= (2048))
        #else
                if (s_download.ui32_chunk_offset >= s_mesh_update.s_details.ui32_size) // if done downloading
        #endif
                {
                    s_download.e_state = DOWNLOAD_STATE_VERIFY;
                }
                else // next chunk
                {
                    s_download.e_state = DOWNLOAD_STATE_IDLE;
                }
            }
            else
            {
                LOGW("file chunk error");
                s_mesh_update.ms_timeout = millis();
                s_download.e_state       = DOWNLOAD_STATE_RETRY_DELAY;
            }
        }
        else if (millis() - s_mesh_update.ms_timeout > (10 * 1000UL))
        {
            LOGW("response timeout");
            s_mesh_update.ms_timeout = millis();
            s_download.e_state       = DOWNLOAD_STATE_RETRY_DELAY;
        }
        break;

    case DOWNLOAD_STATE_VERIFY:
        LOGI("download done");
        s_download.ui32_chunk_offset = s_mesh_update.s_details.ui32_size;
        if (true == verifyFileDownload())
        {
            LOGD("verify ok");
            (void)f_sync(&s_download.s_file); // flush
            (void)f_close(&s_download.s_file);

            // prevent conflict with "cloud_update" task
            (void)nvmemClose(NVMEM_FILE_UPDATE);
            (void)f_unlink(K_APP_UPDATE_FILE);

            if (FR_OK == f_rename(K_MESH_DOWNLOAD_TMP_FILENAME, K_APP_UPDATE_FILE))
            {
                s_mesh_update.e_result = UPDATE_RESULT_UPDATE_STARTED;
            }
            else
            {
                LOGE("failed to rename to %s", K_APP_UPDATE_FILE);
                s_mesh_update.e_result = UPDATE_RESULT_INTERNAL_ERROR;
            }
        }
        else
        {
            LOGD("verify failed");
            s_mesh_update.e_result = UPDATE_RESULT_HASH_MISMATCH_ERROR;
            (void)f_close(&s_download.s_file);
            (void)f_unlink(K_MESH_DOWNLOAD_TMP_FILENAME);
        }    
        s_download.e_state = DOWNLOAD_STATE_IDLE;
        break;

    case DOWNLOAD_STATE_RETRY_DELAY:
        if (millis() - s_mesh_update.ms_timeout > K_MESH_CLOUD_REPORT_RETRY_DELAY)
        {
            if (UPDATE_RESULT_UNKNOWN != s_mesh_update.e_result)
            {
                b_status = false; // got error
            }
            else if (++s_download.ui16_chunk_err >= 20)
            {
                s_mesh_update.e_result = UPDATE_RESULT_DOWNLOAD_ERROR; // max retries
                b_status = false;
            }
            s_download.e_state = DOWNLOAD_STATE_IDLE;
        }
        break;

    default:
        s_download.e_state = DOWNLOAD_STATE_IDLE;
        break;
    }

    return b_status;
}
