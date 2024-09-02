
#ifndef __EDGE_PAYLOAD_H__
#define __EDGE_PAYLOAD_H__


#include "edge_payload_defs.h"
#include "edge_payload_cfg.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Global Structs
 */
typedef struct
{
    uint8_t                 ui8_len;
    uint8_t                 aui8_id[K_PAYLOAD_MAX_DEV_ID_LEN];
} ep_dev_id_st;

typedef struct
{
    ep_protocol_ver_et      e_protocol_ver;
    ep_dev_type_et          e_dev_type;
    ep_dev_id_st            s_dev_id;
    int32_t                 si32_timestamp;
} ep_com_header_st;

typedef struct
{
    ep_monitor_id_et        e_id;
    uint16_t                ui16_value;
} ep_monitor_param_st;

typedef struct
{
    ep_monitor_id_et        e_id;
    uint32_t                ui32_value;
} ep_monitor_param32_st;

typedef struct
{
    ep_phase_index_et       e_phase_index;
    uint8_t                 ui8_param_count;
    uint8_t                 ui8_param32_count;
    ep_monitor_param_st     as_params[K_PAYLOAD_MAX_MON_PARAM_COUNT];
    ep_monitor_param32_st   as_params32[K_PAYLOAD_MAX_MON_PARAM32_COUNT];
} ep_monitor_phase_st;

typedef struct
{
    ep_tag_id_et            e_id;
    uint8_t                 ui8_len;
    const uint8_t           *pui8_value;
} ep_status_tag_st;

typedef struct
{
    ep_event_type_et        e_type;
    uint8_t                 ui8_len;
    const uint8_t           *pui8_content;
} ep_event_content_st;

typedef struct
{
    ep_com_header_st        s_header;
    uint8_t                 ui8_phase_count;
    ep_monitor_phase_st     as_mon_phase[K_PAYLOAD_MAX_MON_PHASE_COUNT];
} ep_monitor_payload_st;

typedef struct
{
    ep_com_header_st        s_header;
    uint8_t                 ui8_tag_count;
    ep_status_tag_st        as_tags[K_PAYLOAD_MAX_STATUS_TAG_COUNT];
} ep_status_payload_st;

typedef struct
{
    ep_com_header_st        s_header;
    uint8_t                 ui8_event_count;
    ep_event_content_st     as_events[K_PAYLOAD_MAX_EVENTS_COUNT];
} ep_event_payload_st;

typedef struct
{
    ep_com_header_st        s_header;
    uint32_t                ui32_chunk_offset;
    uint16_t                ui16_chunk_length;
} ep_update_get_payload_st;

/*
 * Public Function Prototypes
 */
void edgePayloadInit(void);

bool edgePayloadInitComHeader(ep_com_header_st *ps_header, int32_t si32_timestamp);
bool edgePayloadComHeader2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_com_header_st *ps_header);

bool edgePayloadInitMonitor(ep_monitor_payload_st *ps_monitor, const ep_com_header_st *ps_header);
bool edgePayloadNewMonitorPhase(ep_monitor_payload_st *ps_monitor, ep_phase_index_et e_phase);
bool edgePayloadAddMonitorParam(ep_monitor_payload_st *ps_monitor, ep_monitor_id_et e_mon_id, uint16_t ui16_value);
bool edgePayloadAddMonitorParam32(ep_monitor_payload_st *ps_monitor, ep_monitor_id_et e_mon_id, uint32_t ui32_value);
bool edgePayloadMonitor2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_monitor_payload_st *ps_monitor);

bool edgePayloadInitStatus(ep_status_payload_st *ps_status, const ep_com_header_st *ps_header);
bool edgePayloadAddStatusTag(ep_status_payload_st *ps_status, ep_tag_id_et e_tag_id, const uint8_t *pui8_value, uint8_t ui8_len);
bool edgePayloadStatus2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_status_payload_st *ps_status);

bool edgePayloadInitEvent(ep_event_payload_st *ps_event, const ep_com_header_st *ps_header);
bool edgePayloadAddEventContent(ep_event_payload_st *ps_event, ep_event_type_et e_event_type, const uint8_t *pui8_content, uint8_t ui8_len);
bool edgePayloadEvent2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_event_payload_st *ps_event);

bool edgePayloadInitUpdateGet(ep_update_get_payload_st *ps_update_get, const ep_com_header_st *ps_header);
bool edgePayloadSetUpdateGetContent(ep_update_get_payload_st *ps_update_get, uint32_t ui32_chunk_offset, uint16_t ui16_chunk_length);
bool edgePayloadUpdateGet2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_update_get_payload_st *ps_update_get);

#ifdef __cplusplus
}
#endif

#endif // __EDGE_PAYLOAD_H__
