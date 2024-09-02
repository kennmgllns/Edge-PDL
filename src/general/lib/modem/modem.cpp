#include <algorithm>  // std::min

#include "global_defs.h"
#include "cloud_comms_cfg.h"
#include "modem.h"


ATModem::ATModem(uart_port_t port) : m_port(port), m_cmd_state(AT_NORMAL), m_cme_error_code(-1) { }

bool ATModem::init()
{
    LOGD("%s", __PRETTY_FUNCTION__);

    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT
    };

    ESP_ERROR_CHECK(uart_driver_install(m_port, MODEM_UART_RX_FIFO_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(m_port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(m_port, MODEM_UART_TXD_PIN, MODEM_UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    return true;
}

uint16_t ATModem::write(const uint8_t *pui8_data, uint16_t ui16_length)
{
    int res = uart_write_bytes(m_port, pui8_data, ui16_length);
    return res > 0 ? res : 0;
}

uint16_t ATModem::read(uint8_t *pui8_data, uint16_t ui16_max_length, const char *pc_end_pattern,
                        uint32_t ui32_character_timeout_ms, uint32_t ui32_wait_timeout_ms)
{
    uint32_t ms_ticks;
    uint16_t ui16_end_length;
    uint16_t ui16_bytes_read;
    uint16_t ui16_num_bytes; // partial read

    //LOGD("%s(%u)", __func__, ui16_max_length);

    ms_ticks = millis();
    ui16_end_length = (NULL != pc_end_pattern) ? (uint16_t)strlen(pc_end_pattern) : 0;

    ui16_bytes_read = 0;
    while (ui16_max_length > 0)
    {
        int read_res = uart_read_bytes(m_port, pui8_data,
                        std::min(ui16_max_length, (uint16_t)MODEM_COMMS_BUFFER_SIZE),
                        ui32_character_timeout_ms);

        ui16_num_bytes = read_res > 0 ? read_res : 0;
        if (0 == ui16_num_bytes)
        {
            if ((millis() - ms_ticks) >=
                ((0 == ui16_bytes_read) ? ui32_wait_timeout_ms : ui32_character_timeout_ms)) {
                //LOGW("%s - timeout", __func__);
                break; // timeout
            }
            delayms(1);
            continue; // try again
        }

        //LOGD("num bytes %u/%u: %.*s", ui16_num_bytes, ui16_max_length, ui16_num_bytes, pui8_data);

        pui8_data       += ui16_num_bytes;
        ui16_bytes_read += ui16_num_bytes;
        ui16_max_length -= ui16_num_bytes;

        if ((0 != ui16_end_length) &&
            (ui16_num_bytes >= ui16_end_length) &&
            (0 == memcmp(pui8_data - ui16_end_length, pc_end_pattern, ui16_end_length)))
        {
            //if (ui16_end_length > 20) { LOGD("found pattern"); }
            break; // found end-of-data pattern (e.g. CRLF)
        }

        ms_ticks = millis();
    }

    return ui16_bytes_read;
}

uint16_t ATModem::read_line(char **pp_line_buff, uint32_t ui32_wait_timeout_ms)
{
    uint16_t ui16_bytes_read;

    ui16_bytes_read = read(m_rx_buffer, sizeof(m_rx_buffer) - 1,
                            STR_CRLF, MODEM_COMMS_RX_LINE_TIMEOUT, ui32_wait_timeout_ms);

    *pp_line_buff = (char *)m_rx_buffer;

    return ui16_bytes_read;
}

int ATModem::printf(bool b_debug, bool b_flush_rx, cmd_state_et e_state, const char *pc_format, ...)
{
    va_list args;
    int result;

    va_start(args, pc_format);
    result = vsnprintf((char *)m_tx_buffer, sizeof(m_tx_buffer), pc_format, args);
    va_end(args);

    if (result > 0)
    {
        if (b_debug && (result > 2))
        {
            LOGD("cmd: %.*s", result - 2, m_tx_buffer);
        }
        if (b_flush_rx) // clear receive buffer for the modem response
        {
            handle_events(0);
        }
        m_cmd_state = e_state;
        result = write(m_tx_buffer, (uint16_t)result);
    }

    return result;
}

int ATModem::scanf(bool b_debug, const char *pc_end_pattern,
                    uint32_t ui32_character_timeout_ms, uint32_t ui32_wait_timeout_ms,
                    const char *pc_format, ...)
{
    va_list args;
    char *pc_data;
    char *pc_search;
    int result;
    uint16_t ui16_bytes_read;

    //assert(NULL != pc_format);
    //assert(strlen(pc_format) > 0);

    result = -1;
    do {
        memset(m_rx_buffer, 0, sizeof(m_rx_buffer));
        ui16_bytes_read = read(m_rx_buffer, sizeof(m_rx_buffer) - 1, pc_end_pattern,
                                ui32_character_timeout_ms, ui32_wait_timeout_ms);
        //LOGD("rx: %.*s", ui16_bytes_read, m_rx_buffer);

        if (ui16_bytes_read < __builtin_strlen(STR_CRLF))
        {
            //LOGW("%s: no actual response", __func__);
            result = 0;
            break;
        }

        pc_data = (char *)m_rx_buffer;

        // trim trailing CRLF
      #if 0
        if ('\r' == pc_data[ui16_bytes_read - 2]) {
            pc_data[ui16_bytes_read - 2] = '\0';
        }
      #endif

        // trim leading CRLF's
      #if 1
        while (('\r' == pc_data[0]) && ('\n' == pc_data[1])) {
            pc_data         += 2;
            ui16_bytes_read -= 2;
        }
      #else
        while ((ui16_bytes_read > 0) && (*pc_data != '\0') && (*pc_data < ' ')) {
            pc_data++;
            ui16_bytes_read--;
        }
      #endif

        if (!ui16_bytes_read)
        {
            LOGW("%s: empty response", __func__);
            result = 0;
            break;
        }

        if (b_debug)
        {
            LOGD("rsp: %s", pc_data);
        }

        if (0 == strncmp(pc_data, STR_RESP_CME_ERROR, __builtin_strlen(STR_RESP_CME_ERROR)))
        {
            sscanf(pc_data + __builtin_strlen(STR_RESP_CME_ERROR), ": %d", &m_cme_error_code);
            LOGW("CME error %d", m_cme_error_code);
            // set as negative value to indicate an error occured
            result = (m_cme_error_code > 0) ? (-m_cme_error_code) : (m_cme_error_code);
            break;
        }

        m_cme_error_code = -1; // clear cme error

        if (NULL != (pc_search = strchr(pc_format, '%'))) // scan formated string
        {
            va_start(args, pc_format);
            result = vsscanf(pc_data, pc_format, args);
            va_end(args);
        }
        else if (NULL != (pc_search = strstr(pc_data, pc_format)))  // string search only
        {
            result = 1;
        }

        // redundant processing, but it's still ok ...
        parse_urc(pc_data);

    } while (0);

    return result;
}

void CellularModem::init()
{
    ATModem::init();

    memset(&s_info, 0, sizeof(s_info));
    memset(&s_net_status, 0, sizeof(s_net_status));
    memset(&s_cnx_cfg, 0, sizeof(s_cnx_cfg));
    memset(&s_handlers, 0, sizeof(s_handlers));
}

const char *CellularModem::creg_stat_str(int *pn_stat)
{
    const char *pc_status;

    switch (*pn_stat)
    {
    case REG_STAT_NOT_REGISTERED:
        pc_status = "not registered";
        break;
    case REG_STAT_REGISTERED_HOME_NETWORK:
        pc_status = "registered to home network";
        break;
    case REG_STAT_SEARCHING_OPERATOR:
        pc_status = "searching network operator";
        break;
    case REG_STAT_REGISTERATION_DENIED:
        pc_status = "denied registration";
        break;
    case REG_STAT_REGISTERED_ROAMING:
        pc_status = "registered to roaming network";
        break;
    default:
        pc_status = "unknown registration status";
        *pn_stat = REG_STAT_UNKNOWN;
        break;
    }

    return pc_status;
}


bool CellularModem::apn_lookup(const char *pc_sim_imsi, char *pc_apn_buff, uint8_t ui8_apn_idx)
{
    // fix me: consider 3-character MNC
    char ac_imsi_prefix[3 /*mcc*/ + 2 /*mnc*/ + 1 /* '\0' */];
    unsigned long ul_imsi_prefix;

    memset(ac_imsi_prefix, 0, sizeof(ac_imsi_prefix));
    strncpy(ac_imsi_prefix, pc_sim_imsi, sizeof(ac_imsi_prefix) - 1);
    ul_imsi_prefix = strtoul(ac_imsi_prefix, NULL, 10);
    //LOGD("imsi=%s, prefix=%lu", pc_sim_imsi, ul_imsi_prefix);

  #define SET_APN(name)    strncpy(pc_apn_buff, name, MODEM_APN_MAX_STR_LENGTH - 1);

    switch (ul_imsi_prefix)
    {
    /* pattern for multiple APN's:
        case <mccmnc>:
            if (idx == N)
            {
                SET_APN("myapn", N + 1);
            }
    */
    case 20404: // Netherlands and Vodafone GDSP IoT SIMs
        if (ui8_apn_idx == 0)
        {
            SET_APN("internet.gdsp"); // open-access / public apn
        }
        else
        {
            SET_APN(K_CLOUD_PRIVATE_APNAME); // Edge's private apn
        }
        break;

    case 20430: // NL KORE Wirelesss
        SET_APN("data.apn.name");
        break;

    case 24042: //Quectel
        if (ui8_apn_idx == 0)
        {
            SET_APN("internet.cxn"); // existing public APN
        }
        else
        {
            SET_APN("quectel.tn.std"); // new Radius APN (improved)
        }
        break;

    case 50502: // AU Optus 1
    case 50590: // AU Optus 2
        SET_APN("yesinternet");
        break;

    case 50501: // AU Telstra 1
    case 50571: // AU Telstra 2
    case 50572: // AU Telstra 3
    case 50511: // AU Telstra 4
        if (ui8_apn_idx == 0)
        {
            SET_APN("telstra.internet");
        }
        else
        {
            SET_APN("telstra.m2m");
        }
        break;

    case 50503: // AU Vodafone 1
    case 50538: // AU Vodafone 2
        SET_APN("live.vodafone.com");
        break;

    case 51501: // PH Globe 1
    case 51502: // PH Globe 2
        if (ui8_apn_idx == 0)
        {
            SET_APN("internet.globe.com.ph");
        }
        else
        {
            SET_APN("DTMS.globe.com.ph");
        }
        break;

    case 51503: // PH Smart
        if (ui8_apn_idx == 0)
        {
            SET_APN("internet");
        }
        else
        {
            SET_APN("DTMS.smart");
        }
        break;

    case 46000: // CH China Mobile 1
    case 46007: // CH China Mobile 2
        SET_APN("cmnet");
        break;

    case 46001: // CH China UNICOM 1
    case 46003: // CH China UNICOM 2
    case 46006: // CH China UNICOM 3
        SET_APN("3gnet");
        break;

    case 46005: // CH China TELECOM:
        SET_APN("ctnet");
        break;

    case 45412: // HK China MOBILE
        SET_APN("CMHK");
        break;

    case 52000: // TH_CAT_TELECOM_1
    case 52002: // TH_CAT_TELECOM_2
    case 52001: // TH_AIS_1
    case 52023: // TH_AIS_2
    case 52003: // TH_AIS_3G
    case 52084: // TH_TRUE_MOVE_1
    case 52088: // TH_TRUE_MOVE_2
    case 52089: // TH_TRUE_MOVE_3
        SET_APN("internet");
        break;

    case 72418:
        SET_APN("quectel.br");
        break;

    default:
        SET_APN("internet");
        break;
    }

    LOGD("mccmnc=%s apn=\"%s\"", ac_imsi_prefix, pc_apn_buff);
    return true;
}

uint32_t CellularModem::encode_plmn(int mcc, int mnc)
{
    char     tmp[8];
    uint32_t plmn = 0;

    snprintf(tmp, sizeof(tmp) - 1, "%03d%02d", mcc, mnc);

    // plmn byte 1 = nibbles mcc2 + mcc1
    plmn  = (tmp[1] - '0') << (4 + 16);
    plmn += (tmp[0] - '0') << (0 + 16);
    // plmn byte 2 = nibbles 0xF + mcc3
    plmn += (0xf) << (4 + 8); // fix me: support for 3-digit mnc
    plmn += (tmp[2] - '0') << (0 + 8);
    // plmn byte 3 = nibbles mnc2 + mnc1
    plmn += (tmp[4] - '0') << (4 + 0);
    plmn += (tmp[3] - '0') << (0 + 0);

    return plmn;
}
