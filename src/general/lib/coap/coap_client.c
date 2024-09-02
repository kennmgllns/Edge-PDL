
#include "global_defs.h"
#include "coap_client.h"


#define COAP_HEADER_SIZE            (sizeof(coap_header_st))
#define COAP_OPTION_DELTA(v, n)     (v < 13 ? (*n = (0xFF & v)) : (v <= 0xFF + 13 ? (*n = 13) : (*n = 14)))


/*
 * Private Functions
 */
static void packetAddOption(coap_packet_st *ps_packet, uint8_t ui8_optnumber, uint8_t ui8_length, const uint8_t *opt_payload)
{
    ps_packet->as_options[ps_packet->ui8_optioncount].e_option = ui8_optnumber;
    ps_packet->as_options[ps_packet->ui8_optioncount].ui8_len  = ui8_length;
    ps_packet->as_options[ps_packet->ui8_optioncount].pui8_ptr = opt_payload;

    ++ps_packet->ui8_optioncount;
}

static int parseOption(coap_option_st *option, uint16_t *running_delta, const uint8_t **buf, size_t buflen)
{
    const uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len, delta;

    if (buflen < headlen) {
        return -1;
    }

    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    if (delta == 13)
    {
        headlen++;
        if (buflen < headlen)
            return -1;
        delta = p[1] + 13;
        p++;
    }
    else if (delta == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return -1;
        delta = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    }
    else if (delta == 15) 
    {
        return -1;
    }

    if (len == 13)
    {
        headlen++;
        if (buflen < headlen)
            return -1;
        len = p[1] + 13;
        p++;
    }
    else if (len == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    }
    else if (len == 15)
    {
        return -1;
    }

    if ((p + 1 + len) > (*buf + buflen)) {
        return -1;
    }
    option->e_option = delta + *running_delta;
    option->pui8_ptr = p+1;
    option->ui8_len  = len;
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

/*
 * Public Functions
 */

/* send a request without any token! */
bool coapClientSendRequest(coap_client_context_st *ps_client_ctx, coap_method_et e_method, uint16_t ui16_msg_id, const char *pc_path, const uint8_t *pui8_payload, uint16_t ui16_payloadlen)
{
    coap_packet_st s_packet;
    uint8_t *p              = NULL;
    uint32_t optdelta;
    uint8_t  len, delta;
    uint16_t running_delta  = 0;
    uint16_t packetSize     = 0;

    //LOGD("%s: %u %s (%u-%u)", __func__, e_method, pc_path, ui16_msg_id, ui16_payloadlen);

    memset(&s_packet, 0, sizeof(s_packet));
    s_packet.s_header.ui2_version   = COAP_VERSION;
    s_packet.s_header.ui2_type      = COAP_TYPE_CONFIRMABLE;
  //s_packet.s_header.ui4_tokenlen  = 0; // no token!
    s_packet.s_header.ui8_code      = e_method;
    s_packet.s_header.ui8_msg_id_h  = ui16_msg_id>>8;
    s_packet.s_header.ui8_msg_id_l  = ui16_msg_id & 0xff;

    // make coap packet base header
    p = ps_client_ctx->aui8_buffer;
    memcpy(p, &s_packet.s_header, sizeof(s_packet.s_header));

    // if this fails, please fix 'coap_header_st' struct
    configASSERT((4 == COAP_HEADER_SIZE) && (((COAP_VERSION<<6)|(COAP_TYPE_CONFIRMABLE<<4)) == p[0]));

    p          += COAP_HEADER_SIZE;
    packetSize += COAP_HEADER_SIZE;

#if 0 // provision for token
    if ((s_packet.pui8_token != NULL) && (s_packet.s_header.ui4_tokenlen <= 0x0F)) {
        memcpy(p, s_packet.pui8_token, s_packet.s_header.ui4_tokenlen);
        p += s_packet.s_header.ui4_tokenlen;
        packetSize += s_packet.s_header.ui4_tokenlen;
    }
#endif

    packetAddOption(&s_packet, COAP_OPT_URI_PATH, strlen(pc_path), (const uint8_t *)pc_path);

    // make option header
    for (int i = 0; i < s_packet.ui8_optioncount; i++)  {
        if (packetSize + 5 + s_packet.as_options[i].ui8_len >= COAP_BUF_MAX_SIZE) {
            return false;
        }
        optdelta = s_packet.as_options[i].e_option - running_delta;
        COAP_OPTION_DELTA(optdelta, &delta);
        COAP_OPTION_DELTA((uint32_t)s_packet.as_options[i].ui8_len, &len);

        *p++ = (0xFF & (delta << 4 | len));
        if (delta == 13) {
            *p++ = (optdelta - 13);
            packetSize++;
        } else if (delta == 14) {
            *p++ = ((optdelta - 269) >> 8);
            *p++ = (0xFF & (optdelta - 269));
            packetSize+=2;
        } if (len == 13) {
            *p++ = (s_packet.as_options[i].ui8_len - 13);
            packetSize++;
        } else if (len == 14) {
            *p++ = (s_packet.as_options[i].ui8_len >> 8);
            *p++ = (0xFF & (s_packet.as_options[i].ui8_len - 269));
            packetSize+=2;
        }

        memcpy(p, s_packet.as_options[i].pui8_ptr, s_packet.as_options[i].ui8_len);
        p += s_packet.as_options[i].ui8_len;
        packetSize += s_packet.as_options[i].ui8_len + 1;
        running_delta = s_packet.as_options[i].e_option;
    }

    // make payload
    if (ui16_payloadlen > 0) {
        if ((packetSize + 1 + ui16_payloadlen) >= COAP_BUF_MAX_SIZE) {
            LOGW("not enough buffer %u/%u", (packetSize + 1 + ui16_payloadlen), COAP_BUF_MAX_SIZE);
            return false;
        }
        *p++ = COAP_PAYLOAD_MARKER;
        memcpy(p, pui8_payload, ui16_payloadlen);
        packetSize += 1 + ui16_payloadlen;
    }

    if (NULL != ps_client_ctx->fpi_send_handler)
    {
        (void)ps_client_ctx->fpi_send_handler(ps_client_ctx->aui8_buffer, packetSize);
    }

    return true;
}

bool coapClientHandleMsg(coap_client_context_st *ps_client_ctx, const uint8_t *pui8_msg, uint16_t ui16_msg_len)
{
    coap_packet_st s_packet;
    // parse coap packet header
    memcpy(&s_packet.s_header, pui8_msg, sizeof(s_packet.s_header));

    if (ui16_msg_len < COAP_HEADER_SIZE || (s_packet.s_header.ui2_version != COAP_VERSION)) {
        //LOGW("invalid packet");
        return false;
    }
  
    if (s_packet.s_header.ui4_tokenlen == 0) {
        s_packet.pui8_token = NULL;
    }
    else if (s_packet.s_header.ui4_tokenlen <= 8)  {
        s_packet.pui8_token = pui8_msg + 4;
    }
    else {
        //LOGW("invalid token");
        return false;
    }

    // parse packet options/payload
    if (COAP_HEADER_SIZE + s_packet.s_header.ui4_tokenlen < ui16_msg_len)
    {
        int optionIndex = 0;
        uint16_t delta = 0;
        const uint8_t *end = pui8_msg + ui16_msg_len;
        const uint8_t *p = pui8_msg + COAP_HEADER_SIZE + s_packet.s_header.ui4_tokenlen;
        while(optionIndex < COAP_MAX_OPTION_NUM && *p != 0xFF && p < end) {
            if (0 != parseOption(&s_packet.as_options[optionIndex], &delta, &p, end-p)) {
                //LOGW("invalid option");
                return false;
            }
            optionIndex++;
        }
        s_packet.ui8_optioncount = optionIndex;

        if (p+1 < end && *p == COAP_PAYLOAD_MARKER) {
            s_packet.pui8_payload    = p+1;
            s_packet.ui16_payloadlen = end-(p+1);
        } else {
            //LOGW("empty payload");
            s_packet.pui8_payload    = NULL;
            s_packet.ui16_payloadlen = 0;
        }
    }

    //LOGD("got response type %u", s_packet.s_header.ui2_type);
    if (NULL != ps_client_ctx->fpv_resp_handler)
    {
        // call response function
        ps_client_ctx->fpv_resp_handler(&s_packet);
    }

    return true;
}