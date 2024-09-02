
#include "global_defs.h"
#include "quectel.h"


// read internet connection configuration
bool QuectelModem::get_connection_config(pdp_ctx_et id_ctx)
{
    char    ac_temp_apn[MODEM_APN_MAX_STR_LENGTH]; // cgdcont apn
    char    ac_temp_protocol[8];
    char   *p_tmp; // temporary pointer
    char   *p_cfg; // start of config line
    char   *pc_save; // token save
    int     n_cid; // cgdcont context id
    int     n_ctx_type;
    bool    b_result = false;

    if (!sendAT(this, K_MODEM_STR_CMD_CONFIG_PDP_CONTEXT "=%d", id_ctx))
    {
        //
    }
    else if (waitResponse(K_MODEM_STR_CMD_CONFIG_PDP_CONTEXT ": %d,\"%63[^\"]\"",
                        &n_ctx_type, s_cnx_cfg.ac_apn) < 2)
    {
        LOGW("failed to read pdp config %d", id_ctx);
    }
    // check also with standard command
    else if (!sendAT(this, K_MODEM_STR_CMD_DEFINE_PDP_CONTEXT "?"))
    {
        //
    }
    else
    {
        delayms(500);
        if (read_line(&p_tmp, 500UL) < __builtin_strlen(K_MODEM_STR_CMD_DEFINE_PDP_CONTEXT))
        {
            //LOGW("no " K_MODEM_STR_CMD_DEFINE_PDP_CONTEXT " response");
        }
        else if (NULL == (p_cfg = strstr(p_tmp, K_MODEM_STR_CMD_DEFINE_PDP_CONTEXT)))
        {
            //LOGW("no " K_MODEM_STR_CMD_DEFINE_PDP_CONTEXT " response");
        }
        else
        {
            p_cfg = strtok_r(p_cfg, STR_CRLF, &pc_save);
            while (NULL != p_cfg)
            {
                //LOGD("%s", p_cfg);
                if (3 == sscanf(p_cfg, K_MODEM_STR_CMD_DEFINE_PDP_CONTEXT ": %d,\"%7[^\"]\",\"%63[^\"]\"",
                                &n_cid, ac_temp_protocol, ac_temp_apn))
                {
                    if (n_cid == id_ctx)
                    {
                        b_result = (0 == strncmp(s_cnx_cfg.ac_apn, ac_temp_apn, sizeof(ac_temp_apn)));
                        //LOGD("cgdcont=%d apn=%s (res=%d)", n_cid, ac_temp_apn, b_result);
                        break;
                    }
                }
                else
                {
                    parse_urc(p_cfg); // check for other responses
                }

                p_cfg = strtok_r(NULL, STR_CRLF, &pc_save);
            }
        }
    }

    return b_result;
}

bool QuectelModem::set_connection_config(pdp_ctx_et id_ctx, const char *pc_apn)
{
    bool b_result = false;

    // configure first with standard command
    if (!sendAT(this, K_MODEM_STR_CMD_DEFINE_PDP_CONTEXT "=%d,\"IP\",\"%s\"", id_ctx, pc_apn))
    {
        //
    }
    else if (waitOK() < 1)
    {
        LOGW("CGDCONT apn %d: \"%s\" failed", id_ctx, pc_apn);
        (void)show_pdp_error();
    }

    // then configure also with proprietary command (must be consisten apn value)
    if (!sendAT(this, K_MODEM_STR_CMD_CONFIG_PDP_CONTEXT "=%d,%d,\"%s\"", id_ctx, CTX_IPV4, pc_apn))
    {
        //
    }
    else if (waitOK() < 1)
    {
        LOGW("QICSGP apn %d: \"%s\" failed", id_ctx, pc_apn);
        (void)show_pdp_error();
    }
    else
    {
        b_result = true;
    }

    return b_result;
}

// bring up the PDP connection
bool QuectelModem::activate_pdp(pdp_ctx_et id_ctx)
{
    bool b_result = false;

    if (printf(MODEM_COMMS_AT_CMD_DEBUG, true, AT_WAIT_RESPONSE,
                    "AT" K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT "=%d\r\n", id_ctx) < __builtin_strlen(K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT))
    {
        //
    }
    else
    {
        b_result = true;
        // manually poll the delayed response
    }

    return b_result;
}

bool QuectelModem::deactivate_pdp(pdp_ctx_et id_ctx)
{
    bool b_result = false;

    if (!sendAT(this, K_MODEM_STR_CMD_DEACTIVATE_PDP_CONTEXT "=%d", id_ctx))
    {
        //
    }
    else if (waitResponseTimeout(10 * 1000UL, STR_RESP_OK) < 1)
    {
        //(void)show_pdp_error();
    }
    else
    {
        b_result = true;
    }

    return b_result;
}

// fill up context id & ipv4 address
bool QuectelModem::get_active_connection()
{
    char   *p_tmp; // temporary pointer
    char   *p_info; // context info line
    char   *pc_save; // token save
    int     n_type;
    bool    b_result = false;

    if (!sendAT(this, K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT "?"))
    {
        //
    }
    else
    {
        delayms(500);
        if (read_line(&p_tmp, 500UL) < __builtin_strlen(K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT))
        {
            //LOGW("no " K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT " response");
        }
        else if (NULL == (p_info = strstr(p_tmp, K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT)))
        {
            //LOGW("no " K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT " response");
        }
        else
        {
            p_info = strtok_r(p_info, STR_CRLF, &pc_save);
            while (NULL != p_info)
            {
                //LOGD("%s", p_info);
                if (4 == sscanf(p_info, K_MODEM_STR_CMD_ACTIVATE_PDP_CONTEXT ": %d,%d,%d,\"%31[^\"]\"",
                                &s_cnx_cfg.id_ctx, &s_cnx_cfg.n_ctx_state, &n_type, s_cnx_cfg.ac_address))
                {
                    if (CTX_ACTIVATED == s_cnx_cfg.n_ctx_state)
                    {
                        //LOGD("found active ctx=%d", s_cnx_cfg.id_ctx);
                        b_result = true;
                        break;
                    }
                }
                else
                {
                    parse_urc(p_info); // check for other responses
                }

                p_info = strtok_r(NULL, STR_CRLF, &pc_save);
            }
        }
    }

    return b_result;
}

bool QuectelModem::config_pdp_options()
{
    int     n_recvind = 0;
    bool    b_result = false;

    if (!sendAT(this, K_MODEM_STR_CMD_PDP_CONFIG_OPT_PARAMS "=\"recvind\""))
    {
        //
    }
    else if (waitResponse(K_MODEM_STR_CMD_PDP_CONFIG_OPT_PARAMS ": \"recvind\",%d", &n_recvind) < 1)
    {
        //
    }
    else if (n_recvind < 1)
    {
        // enable receive data length indication in URC
        (void)sendAT(this, K_MODEM_STR_CMD_PDP_CONFIG_OPT_PARAMS "=\"recvind\",%d", 1 /*enable*/);
        b_result = (waitOK() > 0);
    }
    else
    {
        b_result = true;
    }

    return b_result;
}

uint16_t QuectelModem::retrieve_pdp_data(int id_conn, uint8_t **ppui8_buff /*in=buff and out=data*/, uint16_t ui16_buff_size)
{
  #define READ_DATA_PREFIX        "\r\n" K_MODEM_STR_CMD_PDP_RECEIVE_DATA ": %u\r\n"
  #define READ_DATA_SUFFIX        "\r\nOK\r\n"
  #define READ_MIN_LENGTH         (__builtin_strlen(READ_DATA_PREFIX) + __builtin_strlen(READ_DATA_SUFFIX))

    uint8_t *pui8_data          = *ppui8_buff;
    uint32_t ms_ticks           = millis();
    uint16_t ui16_bytes_read    = 0;
    uint16_t ui16_max_len       = ui16_buff_size - READ_MIN_LENGTH;

    if (!sendAT(this, K_MODEM_STR_CMD_PDP_RECEIVE_DATA"=%d,%lu", id_conn, ui16_max_len))
    {
        //
    }
    else
    {
        while (ui16_bytes_read < ui16_max_len)
        {
            uint16_t u16_read = read(pui8_data, ui16_max_len - ui16_bytes_read, NULL, 10, 10);
            if (0 != u16_read)
            {
                pui8_data       += u16_read;
                ui16_bytes_read += u16_read;

                unsigned n_actual_len = 0;
                if (1 != sscanf((const char *)(*ppui8_buff), READ_DATA_PREFIX, &n_actual_len))
                {
                    //LOGD("no actual length yet");
                }
                else if (0 == n_actual_len)
                {
                    //LOGD("empty buffer");
                    ui16_bytes_read = 0; // empty
                    break;
                }
                else
                {
                    // expected prefix/header length
                    uint16_t ui16_prefix_len = __builtin_strlen(READ_DATA_PREFIX);

                    // count length digits
                    if (n_actual_len > 99) {
                        ui16_prefix_len++; // 100 to 999 only
                    } else if (n_actual_len < 10) {
                        ui16_prefix_len--; // 1 to 9
                    } // else 10 to 99

                    // expected raw length with prefix & suffix
                    uint16_t ui16_total_len = ui16_prefix_len + n_actual_len + __builtin_strlen(READ_DATA_SUFFIX);

                    if (ui16_bytes_read >= ui16_total_len)
                    {
                        *ppui8_buff += ui16_prefix_len; // point output to the start of actual data
                        ui16_bytes_read = n_actual_len;
                      #if 0
                        LOGD("%s: got %u bytes", __func__, ui16_bytes_read);
                        LOGB(*ppui8_buff, ui16_bytes_read);
                      #endif
                        break;
                    }
                    //else
                    //{
                    //    LOGD("raw len %u/%u", ui16_bytes_read, ui16_total_len);
                    //}
                }

                ms_ticks = millis();
            }
            else if (millis() - ms_ticks > MODEM_COMMS_RX_LINE_TIMEOUT)
            {
                ui16_bytes_read = 0; // timeout
                break;
            }
        }
    }

    return ui16_bytes_read;
}

int QuectelModem::show_pdp_error()
{
    char ac_desc[32] = {0, };
    int err = 0;

    if (!sendAT(this, K_MODEM_STR_CMD_GET_PDP_ERROR_CODE))
    {
        //
    }
    else if (waitResponse(K_MODEM_STR_CMD_GET_PDP_ERROR_CODE ": %d,%31[^\n]", &err, ac_desc) < 2)
    {
        err = 0;
    }
    else
    {
        m_cme_error_code = err;
        LOGW("err %d: %.*s", err, sizeof(ac_desc), ac_desc);
    }

    return err;
}

int QuectelModem::last_pdp_error()
{
    // to do: define error codes
    return ((m_cme_error_code >= 550) && (m_cme_error_code <= 574)) ? m_cme_error_code : 0;
}
