/*****************************************************************************************//**
* \file         edge_payload.c
*
* \brief        This is a template for C source file.
* \details      This is a template for C source file.
*
* \author       Keith A. Beja
* \version      v00.02.00
* \date         20180108
*
* \copyright    &copy; 2016 Power Quality Engineering Ltd. <br>
*               www.edgeelectrons.com
*********************************************************************************************/
/*
 * Included Modules
 */
#include "global_defs.h"

#include "device_id/device_id.h"

#include "edge_payload_cfg.h"
#include "edge_payload.h"

/*
 * Local Constants
 */
static const uint8_t                UI8_PAYLOAD_MAX_MON_PARAM_COUNT     = (uint8_t)K_PAYLOAD_MAX_MON_PARAM_COUNT;
static const uint8_t                UI8_PAYLOAD_MAX_MON_PHASE_COUNT     = (uint8_t)K_PAYLOAD_MAX_MON_PHASE_COUNT;
static const uint8_t                UI8_PAYLOAD_MAX_STATUS_TAG_COUNT    = (uint8_t)K_PAYLOAD_MAX_STATUS_TAG_COUNT;
static const uint8_t                UI8_PAYLOAD_MAX_EVENTS_COUNT        = (uint8_t)K_PAYLOAD_MAX_EVENTS_COUNT;

static const ep_protocol_ver_et     E_PAYLOAD_PROTOCOL_VER              = (ep_protocol_ver_et)K_PAYLOAD_PROTOCOL_VER;
static const ep_dev_type_et         E_PAYLOAD_DEVICE_TYPE               = (ep_dev_type_et)K_PAYLOAD_DEVICE_TYPE;

/*
 * Local Variables
 */
static uint8_t      aui8_dev_id[K_PAYLOAD_MAX_DEV_ID_LEN];
static uint8_t      ui8_dev_id_len;

/*
 * Private Function Prototypes
 */
/* none */


/*
 * Public Functions
 */

void edgePayloadInit(void)
{
    uint8_t aui8_temp_dev_id[K_DEV_ID_LEN];

	memset(aui8_dev_id, 0, sizeof(aui8_dev_id));
    get_device_id(aui8_temp_dev_id);

    if (sizeof(aui8_dev_id) < sizeof(aui8_temp_dev_id))
    {
	    memcpy(aui8_dev_id, aui8_temp_dev_id, sizeof(aui8_dev_id));
	    ui8_dev_id_len = (uint8_t)K_PAYLOAD_MAX_DEV_ID_LEN;
    }
    else
    {
	    memcpy(aui8_dev_id, aui8_temp_dev_id, sizeof(aui8_temp_dev_id));
	    ui8_dev_id_len = (uint8_t)K_DEV_ID_LEN;
    }
}

bool edgePayloadInitComHeader(ep_com_header_st *ps_header, int32_t si32_timestamp)
{
    ps_header->e_protocol_ver   = E_PAYLOAD_PROTOCOL_VER;
    ps_header->e_dev_type       = E_PAYLOAD_DEVICE_TYPE;
    ps_header->si32_timestamp   = si32_timestamp;

    ps_header->s_dev_id.ui8_len = ui8_dev_id_len;
    memcpy(ps_header->s_dev_id.aui8_id, aui8_dev_id, (size_t)ps_header->s_dev_id.ui8_len);

    return true;
}

bool edgePayloadComHeader2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_com_header_st *ps_header)
{
    size_t   sz_req_len;
    uint8_t *pui8_end = pui8_buf;
    bool     b_status = false;

    sz_req_len  = (size_t)(1 + 1 + 1 + ps_header->s_dev_id.ui8_len + 4);                // header: protocol version + device type + device id length + device id bytes + timestamp
    if (sz_buf_len >= sz_req_len)
    {
        *pui8_end++ = (uint8_t)ps_header->e_protocol_ver;
        *pui8_end++ = (uint8_t)ps_header->e_dev_type;

        *pui8_end++ = ps_header->s_dev_id.ui8_len;
        memcpy(pui8_end, ps_header->s_dev_id.aui8_id, (size_t)ps_header->s_dev_id.ui8_len);
        pui8_end +=ps_header->s_dev_id.ui8_len;

        /* lsb first */
        *pui8_end++ = (uint8_t)((ps_header->si32_timestamp)       & 0xFF);
        *pui8_end++ = (uint8_t)((ps_header->si32_timestamp >> 8)  & 0xFF);
        *pui8_end++ = (uint8_t)((ps_header->si32_timestamp >> 16) & 0xFF);
        *pui8_end++ = (uint8_t)((ps_header->si32_timestamp >> 24) & 0xFF);

        if ((size_t)(pui8_end - pui8_buf) == sz_req_len)
        {
            *psz_res_len = sz_req_len;
            b_status = true;
        }
    }

    return b_status;
}

bool edgePayloadInitMonitor(ep_monitor_payload_st *ps_monitor, const ep_com_header_st *ps_header)
{
    memset(ps_monitor, 0, sizeof(ep_monitor_payload_st));
    memcpy(&ps_monitor->s_header, ps_header, sizeof(ep_com_header_st));
    ps_monitor->ui8_phase_count = 0;
    ps_monitor->as_mon_phase[0].e_phase_index = 0;

    return true;
}


bool edgePayloadNewMonitorPhase(ep_monitor_payload_st *ps_monitor, ep_phase_index_et e_phase)
{
    bool b_status = false;

    if (e_phase <= EP_PHASE_INDEX_MAX)
    {
        if (ps_monitor->ui8_phase_count < (UI8_PAYLOAD_MAX_MON_PHASE_COUNT))
        {
            ps_monitor->ui8_phase_count++;
            ps_monitor->as_mon_phase[ps_monitor->ui8_phase_count - 1].e_phase_index = e_phase;
            b_status = true;
        }
    }

    return b_status;
}

bool edgePayloadAddMonitorParam(ep_monitor_payload_st *ps_monitor, ep_monitor_id_et e_mon_id, uint16_t ui16_value)
{
    uint8_t ui8_phase_index = ps_monitor->ui8_phase_count - 1;
    uint8_t ui8_param_index = ps_monitor->as_mon_phase[ui8_phase_index].ui8_param_count;
    bool b_status           = false;

    if (ui8_param_index < UI8_PAYLOAD_MAX_MON_PARAM_COUNT)
    {
        ps_monitor->as_mon_phase[ui8_phase_index].as_params[ui8_param_index].e_id       = e_mon_id;
        ps_monitor->as_mon_phase[ui8_phase_index].as_params[ui8_param_index].ui16_value = ui16_value;
        ps_monitor->as_mon_phase[ui8_phase_index].ui8_param_count++;
        b_status = true;
    }

    return b_status;
}

bool edgePayloadAddMonitorParam32(ep_monitor_payload_st *ps_monitor, ep_monitor_id_et e_mon_id, uint32_t ui32_value)
{
    uint8_t ui8_phase_index = ps_monitor->ui8_phase_count - 1;
    uint8_t ui8_param_index = ps_monitor->as_mon_phase[ui8_phase_index].ui8_param32_count;
    bool b_status           = false;

    if (ui8_param_index < UI8_PAYLOAD_MAX_MON_PARAM_COUNT)
    {
        ps_monitor->as_mon_phase[ui8_phase_index].as_params32[ui8_param_index].e_id         = e_mon_id;
        ps_monitor->as_mon_phase[ui8_phase_index].as_params32[ui8_param_index].ui32_value   = ui32_value;
        ps_monitor->as_mon_phase[ui8_phase_index].ui8_param32_count++;
        b_status = true;
    }

    return b_status;
}

bool edgePayloadMonitor2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_monitor_payload_st *ps_monitor)
{
    uint8_t ui8_phase;
    uint8_t ui8_param;
    uint8_t ui8_param_count;
    size_t  sz_hdr_len;
    size_t  sz_req_len;

    uint8_t *pui8_end = pui8_buf;
    bool b_status = false;

    sz_req_len  = (size_t)(1 + 1 + 1 + ps_monitor->s_header.s_dev_id.ui8_len + 4);              // header: protocol version + device type + device id length + device id bytes + timestamp

    for (ui8_phase = 0; ui8_phase < ps_monitor->ui8_phase_count; ui8_phase++)
    {
        // monitor payload: phase index and parameter count  + 3 bytes per parameter (id + value) + 5 bytes per 32-bit parameter (id + 32-bit value)
        sz_req_len += (size_t)(1 + (3 * ps_monitor->as_mon_phase[ui8_phase].ui8_param_count) + (5 * ps_monitor->as_mon_phase[ui8_phase].ui8_param32_count));
    }

    if ((sz_buf_len >= sz_req_len) &&
        (true == edgePayloadComHeader2Buf(pui8_end, sz_buf_len, &sz_hdr_len, &ps_monitor->s_header)))
    {
        /* header */
        pui8_end += sz_hdr_len;

        /* monitor payload */
        for (ui8_phase = 0; ui8_phase < ps_monitor->ui8_phase_count; ui8_phase++)
        {
            ui8_param_count = ps_monitor->as_mon_phase[ui8_phase].ui8_param_count + ps_monitor->as_mon_phase[ui8_phase].ui8_param32_count;
            *pui8_end++ = (uint8_t)(((ps_monitor->as_mon_phase[ui8_phase].e_phase_index << 5) & 0xE0) | (ui8_param_count & 0x1F));

            for (ui8_param = 0; ui8_param < ps_monitor->as_mon_phase[ui8_phase].ui8_param_count; ui8_param++)
            {
                *pui8_end++ = (uint8_t)(ps_monitor->as_mon_phase[ui8_phase].as_params[ui8_param].e_id);
                *pui8_end++ = (uint8_t)((ps_monitor->as_mon_phase[ui8_phase].as_params[ui8_param].ui16_value)       & 0xFF);
                *pui8_end++ = (uint8_t)((ps_monitor->as_mon_phase[ui8_phase].as_params[ui8_param].ui16_value >> 8)  & 0xFF);
            }

            for (ui8_param = 0; ui8_param < ps_monitor->as_mon_phase[ui8_phase].ui8_param32_count; ui8_param++)
            {
                *pui8_end++ = (uint8_t)(ps_monitor->as_mon_phase[ui8_phase].as_params32[ui8_param].e_id);
                *pui8_end++ = (uint8_t)((ps_monitor->as_mon_phase[ui8_phase].as_params32[ui8_param].ui32_value)         & 0xFF);
                *pui8_end++ = (uint8_t)((ps_monitor->as_mon_phase[ui8_phase].as_params32[ui8_param].ui32_value >> 8)    & 0xFF);
                *pui8_end++ = (uint8_t)((ps_monitor->as_mon_phase[ui8_phase].as_params32[ui8_param].ui32_value >> 16)   & 0xFF);
                *pui8_end++ = (uint8_t)((ps_monitor->as_mon_phase[ui8_phase].as_params32[ui8_param].ui32_value >> 24)   & 0xFF);
            }
        }

        if ((size_t)(pui8_end - pui8_buf) == sz_req_len)
        {
            *psz_res_len = sz_req_len;
            b_status     = true;
        }
    }

    return b_status;
}

bool edgePayloadInitStatus(ep_status_payload_st *ps_status, const ep_com_header_st *ps_header)
{
    memset(ps_status, 0, sizeof(ep_status_payload_st));
    memcpy(&ps_status->s_header, ps_header, sizeof(ep_com_header_st));

    return true;
}

bool edgePayloadAddStatusTag(ep_status_payload_st *ps_status, ep_tag_id_et e_tag_id, const uint8_t *pui8_value, uint8_t ui8_len)
{
    uint8_t ui8_tag_index   = ps_status->ui8_tag_count;
    bool b_status           = false;

    if (ui8_tag_index < UI8_PAYLOAD_MAX_STATUS_TAG_COUNT)
    {
        if ((e_tag_id >= EP_STATUS_TAG_MIN) && (e_tag_id <= EP_STATUS_TAG_MAX))
        {
            ps_status->as_tags[ui8_tag_index].e_id          = e_tag_id;
            ps_status->as_tags[ui8_tag_index].ui8_len       = ui8_len;
            ps_status->as_tags[ui8_tag_index].pui8_value    = pui8_value;
            ps_status->ui8_tag_count++;
            b_status = true;
        }
    }

    return b_status;
}

bool edgePayloadStatus2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_status_payload_st *ps_status)
{
    uint8_t ui8_i;
    size_t  sz_hdr_len = 0;
    size_t  sz_req_len;

    uint8_t *pui8_end = pui8_buf;
    bool b_status = false;

    sz_req_len  = (size_t)(1 + 1 + 1 + ps_status->s_header.s_dev_id.ui8_len + 4);       // header: protocol version + device type + device id length + device id bytes + timestamp
    sz_req_len += (size_t)(1);                                                          // status payload: tag count + length of each tag
    for (ui8_i = 0; ui8_i < ps_status->ui8_tag_count; ui8_i++)
    {
        sz_req_len += (size_t)(1 + 1 + ps_status->as_tags[ui8_i].ui8_len);              // tag: tag id + tag length byte + tag value length
    }

    if ((sz_buf_len >= sz_req_len) &&
        (true == edgePayloadComHeader2Buf(pui8_end, sz_buf_len, &sz_hdr_len, &ps_status->s_header)))
    {
        /* header */
        pui8_end += sz_hdr_len;

        /* status payload */
        *pui8_end++ = ps_status->ui8_tag_count;
        for (ui8_i = 0; ui8_i < ps_status->ui8_tag_count; ui8_i++)
        {
            *pui8_end++ = (uint8_t)ps_status->as_tags[ui8_i].e_id;
            *pui8_end++ = ps_status->as_tags[ui8_i].ui8_len;
            memcpy(pui8_end, ps_status->as_tags[ui8_i].pui8_value, (size_t)ps_status->as_tags[ui8_i].ui8_len);
            pui8_end += ps_status->as_tags[ui8_i].ui8_len;
        }

        if ((size_t)(pui8_end - pui8_buf) == sz_req_len)
        {
            *psz_res_len = sz_req_len;
            b_status = true;
        }
    }

    return b_status;
}

bool edgePayloadInitEvent(ep_event_payload_st *ps_event, const ep_com_header_st *ps_header)
{
    memset(ps_event, 0, sizeof(ep_event_payload_st));
    memcpy(&ps_event->s_header, ps_header, sizeof(ep_com_header_st));

    return true;
}

bool edgePayloadAddEventContent(ep_event_payload_st *ps_event, ep_event_type_et e_event_type, const uint8_t *pui8_content, uint8_t ui8_len)
{
    uint8_t ui8_event_index = ps_event->ui8_event_count;
    bool b_status           = false;

    if (ui8_event_index < UI8_PAYLOAD_MAX_EVENTS_COUNT)
    {
        ps_event->as_events[ui8_event_index].e_type         = e_event_type;
        ps_event->as_events[ui8_event_index].ui8_len        = ui8_len;
        ps_event->as_events[ui8_event_index].pui8_content   = pui8_content;
        ps_event->ui8_event_count++;
        b_status = true;
    }

    return b_status;
}

bool edgePayloadEvent2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_event_payload_st *ps_event)
{
    uint8_t ui8_i;
    size_t  sz_hdr_len = 0;
    size_t  sz_req_len;

    uint8_t *pui8_end = pui8_buf;
    bool b_status = false;

    sz_req_len  = (size_t)(1 + 1 + 1 + ps_event->s_header.s_dev_id.ui8_len + 4);        // header: protocol version + device type + device id length + device id bytes + timestamp
    sz_req_len += (size_t)(1);                                                          // event payload: event count + length of each event content
    for (ui8_i = 0; ui8_i < ps_event->ui8_event_count; ui8_i++)
    {
        sz_req_len += (size_t)(1 + 1 + ps_event->as_events[ui8_i].ui8_len);             // event: event type + event length byte + event content length
    }

    if ((sz_buf_len >= sz_req_len) &&
        (true == edgePayloadComHeader2Buf(pui8_end, sz_buf_len, &sz_hdr_len, &ps_event->s_header)))
    {
        /* header */
        pui8_end += sz_hdr_len;

        /* event payload */
        *pui8_end++ = ps_event->ui8_event_count;
        for (ui8_i = 0; ui8_i < ps_event->ui8_event_count; ui8_i++)
        {
            *pui8_end++ = (uint8_t)ps_event->as_events[ui8_i].e_type;
            *pui8_end++ = ps_event->as_events[ui8_i].ui8_len;
            memcpy(pui8_end, ps_event->as_events[ui8_i].pui8_content, (size_t)ps_event->as_events[ui8_i].ui8_len);
            pui8_end += ps_event->as_events[ui8_i].ui8_len;
        }

        if ((size_t)(pui8_end - pui8_buf) == sz_req_len)
        {
            *psz_res_len = sz_req_len;
            b_status = true;
        }
    }

    return b_status;
}

bool edgePayloadInitUpdateGet(ep_update_get_payload_st *ps_update_get, const ep_com_header_st *ps_header)
{
    memset(ps_update_get, 0, sizeof(ep_update_get_payload_st));
    memcpy(&ps_update_get->s_header, ps_header, sizeof(ep_update_get_payload_st));

    return true;
}

bool edgePayloadSetUpdateGetContent(ep_update_get_payload_st *ps_update_get, uint32_t ui32_chunk_offset, uint16_t ui16_chunk_length)
{
    ps_update_get->ui32_chunk_offset = ui32_chunk_offset;
    ps_update_get->ui16_chunk_length = ui16_chunk_length;

    return true;
}

bool edgePayloadUpdateGet2Buf(uint8_t *pui8_buf, size_t sz_buf_len, size_t *psz_res_len, const ep_update_get_payload_st *ps_update_get)
{
    size_t  sz_hdr_len = 0;
    size_t  sz_req_len;

    uint8_t *pui8_end = pui8_buf;
    bool b_status = false;

    sz_req_len  = (size_t)(1 + 1 + 1 + ps_update_get->s_header.s_dev_id.ui8_len + 4);   // header: protocol version + device type + device id length + device id bytes + timestamp
    sz_req_len += (size_t)(4 + 2);                                                      // 4 bytes chunk offset + 2 bytes chunk length

    if ((sz_buf_len >= sz_req_len) &&
        (true == edgePayloadComHeader2Buf(pui8_end, sz_buf_len, &sz_hdr_len, &ps_update_get->s_header)))
    {
        /* header */
        pui8_end += sz_hdr_len;

        /* update get payload */
        *pui8_end++ = (uint8_t)((ps_update_get->ui32_chunk_offset)       & 0xFF);
        *pui8_end++ = (uint8_t)((ps_update_get->ui32_chunk_offset >> 8)  & 0xFF);
        *pui8_end++ = (uint8_t)((ps_update_get->ui32_chunk_offset >> 16) & 0xFF);
        *pui8_end++ = (uint8_t)((ps_update_get->ui32_chunk_offset >> 24) & 0xFF);
        *pui8_end++ = (uint8_t)((ps_update_get->ui16_chunk_length)       & 0xFF);
        *pui8_end++ = (uint8_t)((ps_update_get->ui16_chunk_length >> 8)  & 0xFF);

        if ((size_t)(pui8_end - pui8_buf) == sz_req_len)
        {
            *psz_res_len = sz_req_len;
            b_status = true;
        }
    }

    return b_status;
}


/*
 * Private Functions
 */
/* none */

/* end of edge_payload.c */
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------
