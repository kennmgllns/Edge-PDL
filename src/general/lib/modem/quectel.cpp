
#include "global_defs.h"
#include "quectel.h"



QuectelModem::QuectelModem(pin_set_func reset, pin_set_func pwr_on, pin_set_func dtr)
    : CellularModem(MODEM_UART_PORT),
    fp_reset_pin(reset), fp_pwr_on_pin(pwr_on), fp_dtr_pin(dtr),
    m_ready(false), m_powerdown(false)
{
    //
}

void QuectelModem::init()
{
    CellularModem::init();

    fp_reset_pin(true); // keep RESET_N pin high; use the PWRKEY (PWR_ON) pin instead (for compatibility w/ BG9x series)
    fp_dtr_pin(true);   // keep DTR pin high
}

void QuectelModem::reset(bool b_full)
{
    m_ready = false;
    m_powerdown = false;

    /* HW reset */
    if (b_full)
    {
        LOGD("hw reset");
        // press power key
        fp_pwr_on_pin(false);
        delayms(1000); // at least 500ms
        fp_pwr_on_pin(true);

    }
    /* SW reset */
    else
    {
        LOGD("sw reset");
        (void)sendAT(this, K_MODEM_STR_CMD_SET_FUNCTIONALITY "=%d,%d", 1, 1); // full functionality
    }
}

void QuectelModem::parse_urc_cereg(const char *pc_cereg)
{
    int n_stat = 0;
    uint32_t ui32_tac = 0;
    uint32_t ui32_cid = 0;
    int n_act = 0;
    int num_info;
    if (NULL != s_handlers.fpv_notif_cereg_cb)
    {
        if ((num_info = sscanf(pc_cereg, K_MODEM_STR_CMD_EPS_REGISTRATION ": %d,\"%lX\",\"%lX\",%d",
                          &n_stat, &ui32_tac, &ui32_cid, &n_act)) > 0)
        {
            if (4 == num_info) {
                if (n_act >=ACT_EMTC) {
                    LOGD("cereg: %s (tac=%lX ci=%lX act=%d)", creg_stat_str(&n_stat), ui32_tac, ui32_cid, n_act);
                }
                s_handlers.fpv_notif_cereg_cb(n_stat, ui32_tac, ui32_cid, n_act);
            } else {
                LOGD("cereg: %s", creg_stat_str(&n_stat));
            }
          #if 0
            if (REG_STAT_UNKNOWN != n_stat) {
                if (REG_STAT_REGISTERATION_DENIED == n_stat) {
                    show_pdp_error();
                }
            }
          #endif
        }
    }
}

void QuectelModem::parse_urc_qiopen(const char *pc_qiopen)
{
    int id_conn;
    int n_err;

    if (2 == sscanf(pc_qiopen, K_MODEM_STR_CMD_PDP_OPEN_SOCKET ": %d,%d", &id_conn, &n_err))
    {
        if (NULL == s_handlers.fpv_notif_session_status_cb)
        {
            //
        }
        else if (0 == n_err)
        {
            s_handlers.fpv_notif_session_status_cb(id_conn, PDP_STATE_CONNECTED);
        }
        else
        {
            LOGW("socker error %d", n_err);
        }
    }
}

void QuectelModem::parse_urc_qiurc(const char *pc_qiurc)
{
    char    ac_event[16] = {0, };
    int     n_id = -1;  // contextID or connectID
    int     n_info = -1; // state or data length
    int     num_info = 0;

    num_info = sscanf(pc_qiurc, K_MODEM_STR_URC_PDP_NOTIF ": \"%15[^\"]\",%d,%d", ac_event, &n_id, &n_info);
    //LOGD("event=%s id=%d info=%d (cnt=%d)", ac_event, n_id, n_info, num_info);

    if (num_info > 0)
    {
        if (0 == strncmp(ac_event, "recv", 4))
        {
            if (NULL != s_handlers.fpv_notif_data_incoming_cb) {
                s_handlers.fpv_notif_data_incoming_cb(n_id, n_info);
            }
        }
        else
        {
            LOGW("not handled: qiurc %s", ac_event);
        }
    }
}

void QuectelModem::parse_urc(char *pc_urc)
{
    char *pc_token = NULL;
    char *pc_save = NULL;

    pc_token = (NULL != pc_urc) ? strtok_r(pc_urc, STR_CRLF, &pc_save) : NULL;
    while (NULL != pc_token)
    {
#if 0 //#if MODEM_COMMS_AT_RSP_DEBUG
        LOGD("line: %s", pc_token);
#endif

#define STARTS_WITH(str)        (0 == strncmp(pc_token, str, __builtin_strlen(str)))
#define PARSE(pattern, parser)  else if (STARTS_WITH(K_MODEM_STR_ ##pattern)) { parse_urc_ ##parser(pc_token); }
#define IGNORE(pattern)         else if (STARTS_WITH(K_MODEM_STR_ ##pattern)) {  }

        if (STARTS_WITH(STR_RESP_OK))
        {
            if (AT_WAIT_RESPONSE == m_cmd_state) {
                m_cmd_state = AT_RESPONSE_OK;
            }
        }
        else if (STARTS_WITH(STR_RESP_ERROR))
        {
            if (AT_WAIT_RESPONSE == m_cmd_state) {
                m_cmd_state = AT_RESPONSE_ERR;
            }
        }
        else if (STARTS_WITH(K_MODEM_STR_URC_READY))
        {
            m_ready = true;
        }
        else if (STARTS_WITH(K_MODEM_STR_URC_POWERDOWN))
        {
            m_powerdown = true;
        }

        PARSE(CMD_EPS_REGISTRATION, cereg)
        PARSE(CMD_PDP_OPEN_SOCKET, qiopen)
        PARSE(URC_PDP_NOTIF, qiurc)


#if 0 //#if MODEM_COMMS_AT_RSP_DEBUG
        // manually handled (waited response)
        IGNORE(CMD_GET_SIGNAL_QUALITY)
        IGNORE(CMD_NETWORK_REGISTRATION)
        IGNORE(CMD_NETWORK_OPERATOR)

        IGNORE(CMD_CONFIG_PDP_CONTEXT)

        IGNORE(CMD_ENGINEERING_MODE)

        // not handled
        else
        {
            LOGW("not handled: %s", pc_token);
        }
#endif

        pc_token = strtok_r(NULL, STR_CRLF, &pc_save);
    }
}

void QuectelModem::handle_events(uint32_t ui32_wait_ms)
{
    uint32_t ms_ticks;
    uint16_t ui16_bytes_read;

    ms_ticks = millis();
    while (true)
    {
        memset(m_rx_buffer, 0, sizeof(m_rx_buffer));
        ui16_bytes_read = read(m_rx_buffer, sizeof(m_rx_buffer) - 1,
                                STR_CRLF, 5, 10); // set character timeout to 5ms
        if (ui16_bytes_read)
        {
            parse_urc((char *)m_rx_buffer);
        }
        else if (millis() - ms_ticks > ui32_wait_ms)
        {
            break;
        }
    }
}

bool QuectelModem::get_modem_info()
{
    bool b_result = false;

    if (!sendAT(this, K_MODEM_STR_CMD_GET_MANUFACTURER))
    {
        //
    }
    else if (waitResponse("%31[0-9a-zA-Z ]", s_info.ac_manufacturer) < 1)
    {
        LOGW("failed to read modem manufacturer info");
    }
    else if (!sendAT(this, K_MODEM_STR_CMD_GET_MODEL))
    {
        //
    }
    else if (waitResponse("%15s", s_info.ac_model) < 1)
    {
        LOGW("failed to read modem model info");
    }
    else if (!sendAT(this, K_MODEM_STR_CMD_GET_REVISION))
    {
        //
    }
    else if (waitResponse("%47s", s_info.ac_revision) < 1)
    {
        LOGW("failed to read modem revision info");
    }
    else if (!sendAT(this, K_MODEM_STR_CMD_GET_IMEI))
    {
        //
    }
    else if (waitResponse("%31s", s_info.ac_imei) < 1)
    {
        LOGW("failed to read modem imei info");
    }
    else
    {
        LOGI("modem: %s %s (%s)", s_info.ac_manufacturer, s_info.ac_model, s_info.ac_revision);
#if 0   // optional module-serial-number
        if (!sendAT(this, K_MODEM_STR_CMD_GET_SERIALNUMBER "=%d?", 3 /*FSN*/))
        {
            //
        }
        else if (waitResponse(K_MODEM_STR_CMD_GET_SERIALNUMBER ": %31s", s_info.ac_serialnumber) < 1)
        {
            LOGW("failed to read modem serial number");
            strncpy(s_info.ac_serialnumber, "unknown", sizeof(s_info.ac_serialnumber) - 1);
        }
        LOGI("imei : %s (S/N %s)", s_info.ac_imei, s_info.ac_serialnumber);
#else // use imei as s/n
        strncpy(s_info.ac_serialnumber, s_info.ac_imei, sizeof(s_info.ac_serialnumber) - 1);
        LOGI("imei : %s", s_info.ac_imei);
#endif

        b_result = true;
    }

    return b_result;
}


bool QuectelModem::get_sim_info()
{
    bool b_result = false;

    if (!sendAT(this, K_MODEM_STR_CMD_GET_ICCID))
    {
        //
    }
    else if (waitResponse(K_MODEM_STR_CMD_GET_ICCID ": %20s", s_info.ac_iccid) < 1)
    {
        LOGW("failed to read sim iccid");
    }
    else if (!sendAT(this, K_MODEM_STR_CMD_GET_IMSI))
    {

    }
    else if (waitResponse("%15s", s_info.ac_imsi) < 1)
    {
        LOGW("failed to read sim imsi");
    }
    else
    {
        LOGI("iccid: %s", s_info.ac_iccid);
        LOGI("imsi : %s", s_info.ac_imsi);
        b_result = true;
    }

    return b_result;
}

void QuectelModem::getInfo(char *ac_serialnumber,char *ac_imei,char *ac_imsi,char *ac_iccid){

}

bool QuectelModem::set_network_bands(const char *pc_gsm, const char *pc_lte, const char *pc_extra)
{
    static const char *no_change = "0";
    char gsmbands[8+4] = {0, };
    char ltebands[24+4] = {0, };
    char extrabands[24+4] = {0, };
    int  n_types = 0;
    bool b_status = false;

    if (!sendAT(this, K_MODEM_STR_CMD_EXT_CONFIGURATION "=\"band\""))
    {
        //
    }
    else if ((n_types = waitResponse(K_MODEM_STR_CMD_EXT_CONFIGURATION ": \"band\",%11[^,\r],%27[^,\r],%27[^\r]", gsmbands, ltebands, extrabands)) < 2)
    {
        LOGW("read bands config failed");
    }
    else if ( (0 != (strcmp(gsmbands, pc_gsm))) || (0 != (strcmp(ltebands, pc_lte))) ||
              ((nullptr != pc_extra) && (0 != (strcmp(extrabands, pc_extra)))) )
    {
        //LOGD("gsm bands: %s vs %s", gsmbands, pc_gsm);
        //LOGD("lte bands: %s vs %s", ltebands, pc_lte);

        if (3 == n_types) {
            (void)sendAT(this, K_MODEM_STR_CMD_EXT_CONFIGURATION "=\"band\",%s,%s,%s,1",
                            pc_gsm ? pc_gsm : no_change, pc_lte ? pc_lte : no_change, pc_extra ? pc_extra : no_change);
        } else {
            (void)sendAT(this, K_MODEM_STR_CMD_EXT_CONFIGURATION "=\"band\",%s,%s",
                            pc_gsm ? pc_gsm : no_change, pc_lte ? pc_lte : no_change);
        }

        if (waitOK() < 1)
        {
            LOGW("set rf bands failed");
        }
        else
        {
            LOGI("set RF bands: %s - %s", pc_gsm ? pc_gsm : "0", pc_lte ? pc_lte : no_change);
            b_status = true;
        }
    }
    else
    {
        LOGD("RF bands: %s - %s", gsmbands, ltebands);
        b_status = true;
    }

    return b_status;
}

bool QuectelModem::get_signal_quality()
{
    int n_rssi;
    int n_ber;
    bool b_result = false;

    if (!sendAT(this, K_MODEM_STR_CMD_GET_SIGNAL_QUALITY))
    {
        //
    }
    else if (2 != waitResponse(K_MODEM_STR_CMD_GET_SIGNAL_QUALITY ": %d,%d", &n_rssi, &n_ber))
    {
        LOGW("failed to read signal quality");
    }
    else
    {
        // to do: convert to dBm - or use the values from the cell info
        //LOGD("raw rssi=%d ber=%d", n_rssi, n_ber);
        b_result = true;
    }

    return b_result;
}

bool QuectelModem::get_network_registration()
{
    bool b_result = false;
#if 0
    if (!sendAT(this, K_MODEM_STR_CMD_NETWORK_REGISTRATION))
    {
        //
    }
    else if (2 == waitResponse(K_MODEM_STR_CMD_NETWORK_REGISTRATION ": %d,%d",
                                &s_net_status.s_registration.n_urc, &s_net_status.s_registration.n_stat))
    {
        b_result = true;
    }
    // retry w/ eps
    else
#endif
    if (!sendAT(this, K_MODEM_STR_CMD_EPS_REGISTRATION "?"))
    {
        //
    }
    else if (2 == waitResponse(K_MODEM_STR_CMD_EPS_REGISTRATION ": %d,%d",
                              &s_net_status.s_registration.n_urc, &s_net_status.s_registration.n_stat))
    {
        b_result = true;
    }

    if (true == b_result)
    {
        LOGD("creg: %s", creg_stat_str(&s_net_status.s_registration.n_stat));
    }
    else
    {
        LOGW("failed to read registration status");
    }

    return b_result;
}

bool QuectelModem::get_operator_info(int n_mode, int n_format)
{
    const char *pc_ops_status;
    char   *pc_tmp; // temporary pointer
    char   *pc_ops; // start of cops response
    int     n_stat_or_mode;
    bool    b_result = false;

    memset(&s_net_status.s_operator, 0, sizeof(s_net_status.s_operator));
    s_net_status.s_operator.n_format = -1;

    if (!sendAT(this, K_MODEM_STR_CMD_NETWORK_OPERATOR "?"))
    {
        //
    }
    else if (read_line(&pc_tmp, 1000UL) < __builtin_strlen(K_MODEM_STR_CMD_NETWORK_OPERATOR))
    {
        //LOGW("no " K_MODEM_STR_CMD_NETWORK_OPERATOR " response");
    }
    else if ((NULL == (pc_ops = strstr(pc_tmp, K_MODEM_STR_CMD_NETWORK_OPERATOR))) ||
             (NULL == (pc_ops = strtok(pc_ops, STR_CRLF))))
    {
        //LOGW("no " K_MODEM_STR_CMD_NETWORK_OPERATOR " response");
    }
    else if (sscanf(pc_ops, K_MODEM_STR_CMD_NETWORK_OPERATOR ": %d,%d,\"%31[^\"]\",%d",
                            &n_stat_or_mode, &s_net_status.s_operator.n_format,
                            s_net_status.s_operator.ac_name, &s_net_status.s_operator.n_act) < 4)
    {
#if MODEM_COMMS_AT_RSP_DEBUG
        LOGD("rsp: %s", pc_ops);
#endif
        if (nullptr == strchr(pc_ops, ',')) // if "+COPS: <stat>" info only
        {
            switch (n_stat_or_mode)
            {
            case OPS_STAT_OPERATOR_AVAILABLE:
                pc_ops_status = "operator available";
                break;
            case OPS_STAT_CURRENT_OPERATOR:
                pc_ops_status = "registered";
                break;
            case OPS_STAT_FORBIDDEN_OPERATOR:
                pc_ops_status = "forbidden operator";
                break;
            default:
                pc_ops_status = "unknown operators";
                n_stat_or_mode = OPS_STAT_UNKNOWN;
                break;
            }
            s_net_status.s_operator.n_stat = n_stat_or_mode;
            LOGD("cops: %s", pc_ops_status);
        }
        else if (in_serving_cell() && ((n_mode != n_stat_or_mode) || (n_format != s_net_status.s_operator.n_format)))
        {
            if ((4 /*maunal+auto*/ == n_mode) && (2 /*numeric*/ == n_format))
            {
                LOGD("cops: set manual+auto mode on operator %03d%02d", s_net_status.s_cell.n_mcc, s_net_status.s_cell.n_mnc);
                (void)sendAT(this, K_MODEM_STR_CMD_NETWORK_OPERATOR "=%d,%d,%03d%02d",
                                    n_mode, n_format, s_net_status.s_cell.n_mcc, s_net_status.s_cell.n_mnc);
            }
            else
            {
                LOGD("cops: set mode=%d format=%d", n_mode, n_format);
                (void)sendAT(this, K_MODEM_STR_CMD_NETWORK_OPERATOR "=%d,%d",
                                    (n_mode != n_stat_or_mode) ? n_mode : 3 /*change format only*/, n_format);
            }
            (void)waitOK();
        }
        else
        {
            LOGW("not handled: %s", pc_ops);
        }
    }
    else
    {
        s_net_status.s_operator.n_mode = n_stat_or_mode;
        LOGI("operator=\"%s\" mode=%d AcT=%d", s_net_status.s_operator.ac_name, n_stat_or_mode, s_net_status.s_operator.n_act);
        b_result = '\0' != s_net_status.s_operator.ac_name[0];
    }

    return b_result;
}

bool QuectelModem::get_cell_info()
{
    static char         ac_prev_state[12] = {0, }; // previous UE state
    static uint32_t     ms_last_search = 0;

    char        ac_state[12];   // UE state
    char        ac_rat[12];     // GSM, WCDMA, LTE, eMTC or NBIoT
    char        ac_tdd[12];     // FDD
    char       *p_tmp; // temporary pointer
    char       *p_info; // cell info line
    char       *pc_save; // token save
    int         n_mcc, n_mnc;
    uint32_t    ui32_cid, ui32_tac; // cell_ID and TAC/LAC
    int         n_pcid;
    int         n_arfcn, n_band; // rf band
    int         n_ul_bw, n_dl_bw;   // bandwidth
    int         n_rsrp, n_rsrq, n_rssi, n_sinr, n_srxlev;
    int         num_info;
    bool        b_status = false;

    // clear info
    s_net_status.s_cell.n_cell_type = CELL_UNKNOWN;

    (void)sendAT(this, K_MODEM_STR_CMD_ENGINEERING_MODE "=\"servingcell\"");
    delayms(500);
    if (read_line(&p_tmp, 500UL) < __builtin_strlen(K_MODEM_STR_CMD_ENGINEERING_MODE))
    {
        //LOGW("no " K_MODEM_STR_CMD_ENGINEERING_MODE " response");
    }
    else if (NULL == (p_info = strstr(p_tmp, K_MODEM_STR_CMD_ENGINEERING_MODE)))
    {
        //LOGW("no " K_MODEM_STR_CMD_ENGINEERING_MODE " response");
    }
    else
    {
        p_info = strtok_r(p_info, STR_CRLF, &pc_save);
        while (NULL != p_info)
        {
            if (0 == strncmp(p_info, STR_RESP_OK, 2))
            {
                break;
            }
            //LOGD("%s", p_info);
            // +QENG: "servingcell",<state>,"LTE",<is_tdd>,<mcc>,<mnc>,<cellID>,<pcid>,<earfcn>,<freq_band_ind>,<ul_bandwidth>,<dl_bandwidth>,<tac>,<rsrp>,<rsrq>,<rssi>,<sinr>,<srxlev>
            // e.g. +QENG: "servingcell","LIMSRV","eMTC","FDD",240,42,C511706,6,9435,28,2,2,8,-82,-5,-63,16,57
            if ((num_info = sscanf(p_info, K_MODEM_STR_CMD_ENGINEERING_MODE ": \"servingcell\","
                                    "\"%10[^\"]\",\"%10[^\"]\",\"%10[^\"]\",%d,%d,"
                                    "%lX,%d,%d,%d,%d,%d,%lX,%d,%d,%d,%d,%d",
                                    ac_state, ac_rat, ac_tdd, &n_mcc, &n_mnc,
                                    &ui32_cid, &n_pcid, &n_arfcn /*earfcn*/, &n_band, &n_ul_bw, &n_dl_bw, &ui32_tac,
                                    &n_rsrp, &n_rsrq, &n_rssi, &n_sinr, &n_srxlev)) < 1)
            {
                LOGW("get cell-info failed");
            }
            else if (1 == num_info) // "SEARCH" state
            {
                //LOGD("cell %s", ac_state);
                ms_last_search = millis(); // to add some delay to ignore startup values
            }
            // else if (17 == num_info)
            else if (num_info >= 16) // sometimes there's no "srxlev" value (?)
            {
                s_net_status.s_cell.n_cell_type = CELL_LTE_SERVING;
                s_net_status.s_cell.ui32_plmn   = encode_plmn(n_mcc, n_mnc);

                snprintf(s_net_status.s_rfband.ac_band, sizeof(s_net_status.s_rfband.ac_band) - 1,
                            "B%d-ch%d-bw(%d-%d)", n_band, n_arfcn, n_ul_bw, n_dl_bw);

                // log change in cell info
                if ((0 == strncmp(ac_prev_state, "SEARCH", sizeof(ac_prev_state))) ||
                    (ui32_cid != s_net_status.s_cell.ui32_cid) ||
                    (ui32_tac != s_net_status.s_cell.ui32_tac))
                {
                    LOGI("plmn=(%x,%03d,%02d) cell=(%lx,%lx) rf=(B%d,%d) bw=(%d,%d)",
                        s_net_status.s_cell.ui32_plmn, n_mcc, n_mnc, ui32_cid, ui32_tac, n_band, n_arfcn, n_ul_bw, n_dl_bw);
                }

                s_net_status.s_cell.n_mcc       = n_mcc;
                s_net_status.s_cell.n_mnc       = n_mnc;
                s_net_status.s_cell.ui32_tac    = ui32_tac;
                s_net_status.s_cell.ui32_cid    = ui32_cid;
                s_net_status.s_cell.n_pcid      = n_pcid;

                // log signal levels
                if ((millis() - ms_last_search) > (10*1000UL))
                {
                    s_net_status.s_signal_quality.n_rssi  = n_rssi; // dBm
                    s_net_status.s_signal_quality.n_rsrp  = n_rsrp; // dBm
                    s_net_status.s_signal_quality.n_rsrq  = n_rsrq; // dB
                    s_net_status.s_signal_quality.n_sinr  = n_sinr; // 0 - 250 (1/5) × <SINR> - 20 = dB

                    LOGD("%s(%s) rssi=%d rsrp=%d sinr=%d rsrq=%d", ac_rat, ac_state, n_rssi, n_rsrp, n_sinr, n_rsrq);
                    b_status = true;
                }
            }
            else if (2 == num_info) // parse again with WCMDA mode
            {
                // +QENG: "servingcell",<state>,"WCDMA",<mcc>,<mnc>,<LAC>,<cellID>,<uarfcn>,<psc>,<rac>,<rscp>,<ecio>,<phych>,<SF>,<slot>,<speech_code>,<ComMod>
                // e.g. +QENG: "servingcell","NOCONN","WCDMA",515,03,3AB2,B55EEFF,3037,352,1,-94,-15,-,-,-,-,-
                num_info = sscanf(p_info, K_MODEM_STR_CMD_ENGINEERING_MODE ": \"servingcell\","
                                "\"%10[^\"]\",\"%10[^\"]\",%d,%d,%lX,%lX,%d,%d,%d,%d,%d",
                                ac_state, ac_rat, &n_mcc, &n_mnc, &ui32_tac /*LAC*/, &ui32_cid,
                                &n_arfcn /*uarfcn*/, &n_pcid /*psc - unused*/, &n_srxlev /*rac - unused*/,
                                &n_rsrp /*rscp*/, &n_rsrq /*ecio*/);
                if ((11 == num_info) && (0 == strncmp(ac_rat, "WCDMA", 5)))
                {
                    s_net_status.s_cell.n_cell_type = CELL_UMTS_SERVING;
                    s_net_status.s_cell.ui32_plmn   = encode_plmn(n_mcc, n_mnc);

                    snprintf(s_net_status.s_rfband.ac_band, sizeof(s_net_status.s_rfband.ac_band) - 1, "ch%d", n_arfcn);

                    // log change in cell info
                    if ((0 == strncmp(ac_prev_state, "SEARCH", sizeof(ac_prev_state))) ||
                        (ui32_cid != s_net_status.s_cell.ui32_cid) ||
                        (ui32_tac != s_net_status.s_cell.ui32_tac))
                    {
                        LOGI("plmn=(%x,%03d,%02d) cell=(%lx,%lx) rf=(ch-%d) code=(%d,%d)",
                            s_net_status.s_cell.ui32_plmn, n_mcc, n_mnc, ui32_cid, ui32_tac, n_arfcn, n_pcid, n_srxlev);
                    }

                    s_net_status.s_cell.n_mcc       = n_mcc;
                    s_net_status.s_cell.n_mnc       = n_mnc;
                    s_net_status.s_cell.ui32_tac    = ui32_tac;
                    s_net_status.s_cell.ui32_cid    = ui32_cid;
                    s_net_status.s_cell.n_pcid      = n_pcid;

                    // log signal levels
                    if ((millis() - ms_last_search) > (10*1000UL))
                    {
                        n_rssi = n_rsrp - n_rsrq; // umts: rssi = rscp - ecio
                        s_net_status.s_signal_quality.n_rssi  = n_rssi; // dBm
                        s_net_status.s_signal_quality.n_rsrp  = n_rsrp; // dBm
                        s_net_status.s_signal_quality.n_rsrq  = n_rsrq; // dB
                        s_net_status.s_signal_quality.n_sinr  = n_sinr; // 0 - 250 (1/5) × <SINR> - 20 = dB

                        LOGD("%s(%s) rssi=%d rscp=%d ecio=%d", ac_rat, ac_state, n_rssi, n_rsrp, n_rsrq);
                        b_status = true;
                    }
                }
                else if ((num_info >= 6) && (0 == strncmp(ac_rat, "GSM", 3)))
                {
                    // +QENG: "servingscell",<state>,"GSM",<mcc>,<mnc>,<LAC>,<cellID>,<BSIC>,<arfcn>,<band>,<rxlev>,<txp>,<rla>,<drx>,<c1>,<c2>,<gprs>,<tch>,<ts>,<ta>,<maio>,<hsn>,<rxlevsub>,<rxlevfull>,<rxqualsub>,<rxqualfull>,<voicecodec>
                    // e.g +QENG: "servingcell","NOCONN","GSM",515,03,2BC6,162C,58,40,-,-81,255,255,0,22,-28,1,-,-,-,-,-,-,-,-,-,"-"
                    s_net_status.s_cell.n_cell_type = CELL_GSM_SERVING;
                    s_net_status.s_cell.ui32_plmn   = encode_plmn(n_mcc, n_mnc);

                    s_net_status.s_cell.n_mcc       = n_mcc;
                    s_net_status.s_cell.n_mnc       = n_mnc;
                    s_net_status.s_cell.ui32_tac    = ui32_tac;
                    s_net_status.s_cell.ui32_cid    = ui32_cid;
                    // to do: parse signal info
                    LOGD("%s(%s)", ac_rat, ac_state);
                    b_status = true;
                }
                else
                {
                    LOGW("parse umts cell info failed (num %d)", num_info);
                }
            }
            else
            {
                LOGW("parse lte cell info failed (num %d)", num_info);
            }
            p_info = strtok_r(NULL, STR_CRLF, &pc_save);
        }
    }

    strncpy(ac_prev_state, ac_state, sizeof(ac_prev_state));
    return b_status;
}
