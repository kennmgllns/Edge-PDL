
#pragma once

#include <driver/gpio.h>
#include <driver/uart.h>

#include "modem_cfg.h"

/*
AT-Command Modem class
*/
class ATModem
{
public:
    #define MODEM_STR(str, val)     static inline const char *STR_##str = val

    MODEM_STR(CRLF,             "\r\n");
    MODEM_STR(RESP_OK,          "OK");
    MODEM_STR(RESP_ERROR,       "ERROR");
    MODEM_STR(RESP_CME_ERROR,   "+CME ERROR");

    typedef enum
    {
        AT_NORMAL,          // normal command and/or with immediate response only
      //AT_CME_ERROR,
        // intendended for PDP related commands with very long response time
        AT_WAIT_RESPONSE,   // [blocking] waiting for a response
        AT_RESPONSE_OK,     // got "OK"
        AT_RESPONSE_ERR,    // got "ERROR"
    } cmd_state_et;         // command comms state

    ATModem(uart_port_t port);
    bool init();

    uint16_t write(const uint8_t *pui8_data, uint16_t ui16_length);
    uint16_t read(uint8_t *pui8_data, uint16_t ui16_max_length, const char *pc_end_pattern,
                    uint32_t ui32_character_timeout_ms, uint32_t ui32_wait_timeout_ms);
    int printf(bool b_debug, bool b_flush_rx, cmd_state_et e_state, const char *pc_format, ...);
    int scanf(bool b_debug, const char *pc_end_pattern,
                uint32_t ui32_character_timeout_ms, uint32_t ui32_wait_timeout_ms,
                const char *pc_format, ...);

    /* send command with parameters|arguments (returns true if success) */
    #define sendAT(pdev, fmt, ...)                 ((pdev)->printf(MODEM_COMMS_AT_CMD_DEBUG, true, ATModem::AT_NORMAL, "AT" fmt "\r\n", ## __VA_ARGS__) > __builtin_strlen("AT" fmt))

    /* parse formated response (returns the number of parsed parameters) */
    #define waitResponseTimeout(timeout, fmt, ...)  scanf(MODEM_COMMS_AT_RSP_DEBUG, "OK\r\n", MODEM_COMMS_RX_LINE_TIMEOUT, (timeout), fmt, ## __VA_ARGS__)
    #define waitResponse(fmt, ...)                  waitResponseTimeout(MODEM_COMMS_RX_WAIT_TIMEOUT, fmt, ## __VA_ARGS__)
    #define waitOK()                                waitResponse("OK")


    cmd_state_et cmd_state() const { return m_cmd_state; }
    int get_last_error() const { return m_cme_error_code; }

protected:
    uart_port_t     m_port;
    cmd_state_et    m_cmd_state;
    int             m_cme_error_code;

    uint8_t         m_rx_buffer[MODEM_COMMS_BUFFER_SIZE];
    uint8_t         m_tx_buffer[MODEM_COMMS_BUFFER_SIZE];

    // wrapper for "read()" (class use only)
    uint16_t read_line(char **pp_line_buff, uint32_t ui32_wait_timeout_ms);

    // process unsolicited result codes (URC's), etc.
    virtual void parse_urc(char *pc_urc) = 0;
    virtual void handle_events(uint32_t ui32_wait_ms) = 0;
};


/*
Cellular Network Modem class
*/
class CellularModem : public ATModem
{

public:
    CellularModem(uart_port_t port) : ATModem(port) { }
    void init();

    typedef enum
    {
        REG_STAT_NOT_REGISTERED             = 0,
        REG_STAT_REGISTERED_HOME_NETWORK    = 1,
        REG_STAT_SEARCHING_OPERATOR         = 2,
        REG_STAT_REGISTERATION_DENIED       = 3,
        REG_STAT_UNKNOWN                    = 4,
        REG_STAT_REGISTERED_ROAMING         = 5
    } registration_status_et;  // +CREG|+CEREG stat

    typedef enum
    {
        OPS_STAT_UNKNOWN                    = 0,
        OPS_STAT_OPERATOR_AVAILABLE         = 1,
        OPS_STAT_CURRENT_OPERATOR           = 2,
        OPS_STAT_FORBIDDEN_OPERATOR         = 3
    } operator_status_et;  // +COPS stat

    typedef enum
    {
        // 2G / 3G
        ACT_GSM                             = 0,
        ACT_UTRAN                           = 2,
        ACT_EGPRS                           = 3,
        ACT_HSDPA                           = 4,
        ACT_HSUPA                           = 5,
        ACT_HSDPA_HSUPA                     = 6,
        // 4G
        ACT_EUTRAN                          = 7,
        ACT_EMTC                            = 8, // cat m1
        ACT_NBIOT                           = 9  // nb-iot
    } access_technology_et; // +CREG|+CEREG|+COPS AcT

    typedef enum
    {
        CELL_UNKNOWN                        = -1,
        CELL_GSM_SERVING                    = 0,
        CELL_GSM_NEIGHBOR                   = 1,
        CELL_UMTS_SERVING                   = 2,
        CELL_UMTS_NEIGHBOR                  = 3,
        CELL_UMTS_DETECTED                  = 4,
        CELL_LTE_SERVING                    = 5,
        CELL_LTE_NEIGHBOR                   = 6
    } cell_type_et;

    typedef enum
    {
        PDP_CTX_ID_DEFAULT_APN              = 1,
        PDP_CTX_ID_CUSTOM_APN               = 2,
      //PDP_CTX_ID_BACKUP_APN               = 3
        // ...
        PDP_CTX_ID_MAX_ID                   = PDP_CTX_ID_CUSTOM_APN
    } pdp_ctx_et;  // +QICSGP contextID

    typedef enum
    {
        CTX_UNKNOWN_STATE                   = -1,
        CTX_DEACTIVATED                     = 0,
        CTX_ACTIVATED                       = 1,
    } context_state_et;  // +QIACT context state

    typedef enum
    {
        CTX_UNKNOWN_TYPE                    = 0,
        CTX_IPV4                            = 1,
        CTX_IPV6                            = 2,
        CTX_IPV4_IPV6                       = 3,
    } context_type_et;  // +QICSGP context type

    typedef enum
    {
        PDP_STATE_UNKNOWN                   = -1,
        PDP_STATE_INITIAL                   = 0,
        PDP_STATE_OPENING                   = 1,
        PDP_STATE_CONNECTED                 = 2,
        PDP_STATE_LISTENING                 = 3,
        PDP_STATE_CLOSING                   = 4,
    } pdp_state_et;  // +QISTATE socket state


    typedef enum
    {
        PDP_PROTOCOL_TCP,
        PDP_PROTOCOL_UDP,
        PDP_PROTOCOL_HTTP,
        PDP_PROTOCOL_FTP,
    //PDP_PROTOCOL_HTTPS,
    } pdp_protocol_et;


    typedef struct
    {
        char    ac_manufacturer[31+1];
        char    ac_model[15+1];
        char    ac_serialnumber[31+1]; // optional; use imei instead
        char    ac_revision[47+1];
        char    ac_imei[15+1];
        char    ac_imsi[15+1];
        char    ac_iccid[20+1];
    } information_st;

    typedef struct
    {
        struct { /* "+CSQ?" and "+QCSQ?" */
            int         n_rssi;         // radio signal strength indication
            int         n_ber;          // bit error rate
            int         n_rsrq;         // reference signal received quality
            int         n_rsrp;         // reference signal received power
            int         n_sinr;         // signal to interference+noise ratio
            // rssi stats
            int16_t     si16_rssi_min;
            int16_t     si16_rssi_max;
            int16_t     si16_rssi_acc;  // ave = acc / samples
            uint16_t    ui16_rssi_sample_ctr;

            uint8_t     ui8_signal_fail_cntr;
        } s_signal_quality;
        struct { /* "+CREG" | "+CEREG" */
            int         n_urc;          // enabled unsolicited result code
            int         n_stat;         // network registration status
            uint32_t    ui32_tac;       // location|tracking area code
            uint32_t    ui32_cid;       // cell ID
            int         n_act;          // access technology
        } s_registration;
        struct { /* "+COPS?" */
            int         n_mode;         // registration mode
            int         n_format;       // name format
            char        ac_name[31+1];  // network provider name
            int         n_act;          // access technology
            int         n_stat;         // network operator status
        } s_operator;
        struct { /* +QENG="servingcell" */
            int         n_rat;          // radio access technology
            char        ac_band[24];    // current network band
        } s_rfband;
        struct { /* +QENG="servingcell" */
            int         n_cell_type;    // cell_type_et
            int         n_mcc;          // Mobile country code
            int         n_mnc;          // Mobile network code
            uint32_t    ui32_plmn;      // 3-byte plmn (mcc+mnc)
            uint32_t    ui32_tac;       // tracking area code (lte)
            uint32_t    ui32_cid;       // cell ID (gsm|umts|lte)
            int         n_pcid;         // physical cell id;
        } s_cell;
        uint32_t        ms_last_update; // millisecond timestamp of last status update
    } network_status_st;

    typedef struct
    {
        int             id_ctx;         // PDP context id
        int             n_ctx_state;    // +QIACT <state param> context_state_et
        int             n_pdp_status;   // +QISTATE <socket_state> pdp_state_et
        char            ac_apn[MODEM_APN_MAX_STR_LENGTH]; // access point name
        unsigned        idx_apn;        // APN index (for multiple APN's)
    //char            ac_login[32];
    //char            ac_password[32];
        char            ac_address[32]; // PDP IPv4 address
    //char            ac_eof_pattern[32]; // End of data pattern
        unsigned        num_connect_fail;   // number of failed connection attempts
        unsigned        num_fail_activate;  // number of failed PDP activate
        unsigned        num_fail_apnverify; // number of failed config/verify apn
        uint32_t        ms_last_update; // millisecond timestamp of last config update
    } connection_config_st;

    typedef struct
    {
        // +CEREG
        void (*fpv_notif_cereg_cb)(int n_stat, uint32_t ui32_tac, uint32_t ui32_cid, int n_act);
        // +QIURC: "pdpdeact",<contextID>
        void (*fpv_notif_pdp_status_cb)(int id_ctx, int n_status);
        // +QIOPEN: <connectID> and +QIURC: "closed",<connectID>
        void (*fpv_notif_session_status_cb)(int id_connect, int n_status);
        // +QIURC: "recv",<connectID>,<data_len>
        void (*fpv_notif_data_incoming_cb)(int id_connect, unsigned num_bytes);
    } handlers_st; // call backs (i.e. URC callbacks)


    /* modem context */
    information_st          s_info;
    network_status_st       s_net_status;
    connection_config_st    s_cnx_cfg;
    handlers_st             s_handlers;

    /* helper functions */
    static const char *creg_stat_str(int *pn_stat);
    static bool apn_lookup(const char *pc_sim_imsi, char *pc_apn_buff, uint8_t ui8_apn_idx);
    static uint32_t encode_plmn(int mcc, int mnc);

    bool is_searching_operator() const
    {
        return (REG_STAT_SEARCHING_OPERATOR == s_net_status.s_operator.n_stat);
    }
    bool is_network_registered() const
    {
        return ((REG_STAT_REGISTERED_HOME_NETWORK == s_net_status.s_registration.n_stat) ||
                (REG_STAT_REGISTERED_ROAMING == s_net_status.s_registration.n_stat));
    }
    bool in_serving_cell() const
    {
        return ((CELL_GSM_SERVING == s_net_status.s_cell.n_cell_type) ||
                (CELL_UMTS_SERVING == s_net_status.s_cell.n_cell_type) ||
                (CELL_LTE_SERVING == s_net_status.s_cell.n_cell_type));
    }


    /* to be implemented depending on the manufacturer & model */
    virtual bool get_modem_info() = 0;
    virtual bool get_sim_info() = 0;
    virtual bool set_network_bands(const char *pc_gsm, const char *pc_lte, const char *pc_extra) = 0;
    virtual bool set_lterat_search(int n_cat) = 0;

    virtual bool get_signal_quality() = 0;
    virtual bool get_network_registration() = 0;
    virtual bool get_operator_info(int n_mode, int n_format) = 0;
    virtual bool get_cell_info() = 0;

    virtual bool get_connection_config(pdp_ctx_et id_ctx) = 0;
    virtual bool set_connection_config(pdp_ctx_et id_ctx, const char *pc_apn) = 0;
    virtual bool activate_pdp(pdp_ctx_et id_ctx) = 0;
    virtual bool deactivate_pdp(pdp_ctx_et id_ctx) = 0;
    virtual bool get_active_connection() = 0;
    virtual bool config_pdp_options() = 0;
    virtual int show_pdp_error() = 0;
    virtual int last_pdp_error() = 0;

};
