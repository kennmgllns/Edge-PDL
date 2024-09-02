
#include "global_defs.h"
#include "general_info.h"
#include "modem_manager.h"
#include "modem_manager_cfg.h"


namespace modem
{

MODEM_CLASS mdev(modem_reset_pin, modem_pwr_on_pin, modem_dtr_pin);

namespace manager
{

/*
 * Local Constants
 */
#define K_MODEM_MANAGER_MAX_RESET_PER_HOUR          (10)

/*
 * Local Variables
 */

static state_et                 e_state = STATE_INIT;
static config_state_et          e_config_state = CONFIG_STATE_INIT_CONFIG;
static network_state_et         e_network_state = NETWORK_STATE_GET_SIGNAL_QUALITY;
static pdpctx_state_et          e_pdpctx_state = PDPCTX_STATE_LOOKUP_APN;

static MODEM_CLASS::network_status_st   s_prev_net_status;

static struct {
    uint32_t                    ms_hour;
    uint32_t                    ms_start;
    uint8_t                     u8_count;
} s_reset;

static struct {
    uint8_t                     u8_check_retry;
    //...
    bool                        b_oper_change;
    bool                        b_band_change;
    //...
    bool                        b_custom_apn_ok;
    bool                        b_custom_apn_used;
} s_status;

static SemaphoreHandle_t        mtx_shared_info = NULL; // mutex shared access to modem context
#define LOCK_INFO_ACCESS()      ((NULL != mtx_shared_info) && (pdTRUE == xSemaphoreTake(mtx_shared_info, 15000))) /* wait 15sec */
#define UNLOCK_INFO_ACCESS()    ((void)xSemaphoreGive(mtx_shared_info))


/*
 * Private Function Prototypes
 */
static bool processConfigState(void);
static bool processNetworkState(void);
static bool processDataConnState(void);
static bool processCheckCustomApn(char *pc_apn_buff);

/*
 * Callback Functions
 */
static void update_cereg(int n_stat, uint32_t ui32_tac, uint32_t ui32_ci, int n_act)
{
    //(void)LOCK_INFO_ACCESS(); // ignore failed lock
    mdev.s_net_status.s_registration.n_stat   = n_stat;
    mdev.s_net_status.s_registration.ui32_tac = ui32_tac;
    mdev.s_net_status.s_registration.ui32_cid = ui32_ci;
    mdev.s_net_status.s_registration.n_act    = n_act;
    mdev.s_net_status.s_registration.n_urc    = 1; // non-zero means registration info has changed
    //UNLOCK_INFO_ACCESS();
}

static void update_pdp_connection_status(int id_ctx, int n_status)
{
    LOGD("%s(%d, %d)", id_ctx, n_status);
}

/*
 * Public Functions
 */
bool init()
{
    LOGD("core %u: %s", xPortGetCoreID(), __PRETTY_FUNCTION__);

    mtx_shared_info = xSemaphoreCreateMutex();
    assert(NULL != mtx_shared_info);

    modem_init_pins();

    // enable supply
    modem_vcc_rst_pin(true);
    delayms(100); // at least 30ms

    mdev.init();
    mdev.s_handlers.fpv_notif_cereg_cb          = update_cereg;
    mdev.s_handlers.fpv_notif_pdp_status_cb     = update_pdp_connection_status;
    mdev.s_handlers.fpv_notif_session_status_cb = pdp::handle_session_status;
    mdev.s_handlers.fpv_notif_data_incoming_cb  = pdp::handle_incoming_data;

    e_state = STATE_INIT;

    memset(&s_prev_net_status, 0, sizeof(s_prev_net_status));
    memset(&s_reset, 0, sizeof(s_reset));
    memset(&s_status, 0, sizeof(s_status));

    return true;
}

void cycle()
{
    switch (e_state)
    {
    case STATE_INIT:
        reset(true);
        break;

    case STATE_AT_CHECK:
        if (!sendAT(&mdev, "")) { // "AT\r\n" only
            //
        } else if (mdev.waitOK() < 1) {
            if (++s_status.u8_check_retry > 4) {
                LOGW("init-AT error");
                e_state = STATE_INIT;
                delayms(10*1000UL);
            }
            delayms(3000);
        } else {
            LOGD("init-AT ok");
            setCommsFlag(MCU_TO_MODEM_COMMS, true);
            g_b_mcu_to_modem_ok = true;
            s_status.u8_check_retry = 0;
            e_state                 = STATE_AT_CONFIG;
            e_config_state          = CONFIG_STATE_INIT_CONFIG;
        }
        break;

    case STATE_AT_CONFIG:
        if (false == processConfigState())
        {
            if (++s_status.u8_check_retry > 4) {
                e_state = STATE_INIT;
                delayms(10*1000UL);
            }
            delayms(3000);
        }
        break;

    case STATE_MODEM_INFO:
        if ((0 != mdev.s_info.ac_imei[0]) || (true == mdev.get_modem_info()))
        {
            (void)mdev.set_network_bands(K_MODEM_BANDS_GSM, K_MODEM_BANDS_LTE, K_MODEM_BANDS_EXTRA);
            ac_imei = mdev.s_info.ac_imei;
            ac_serialnumber = mdev.s_info.ac_serialnumber;
            e_state = STATE_SIM_INFO;
        }
        else
        {
            e_state = STATE_AT_CHECK;
            delayms(1000);
        }
        break;

    case STATE_SIM_INFO:
        if ((0 != mdev.s_info.ac_imsi[0]) || (true == mdev.get_sim_info()))
        {
            setCommsFlag(SIM, true);
            g_b_sim_ok = true;
            e_state = STATE_NET_STATUS;
            e_network_state = NETWORK_STATE_GET_SIGNAL_QUALITY;
            (void)mdev.set_lterat_search(0);
            s_status.u8_check_retry = 0;
            ac_imsi = mdev.s_info.ac_imsi;
        }
        else
        {
            if (++s_status.u8_check_retry > 4) {
                LOGW("sim error");
                reset(false);
                delayms(10*1000);
            }
            delayms(1000);
        }
        break;

    case STATE_NET_STATUS:
        if (false == processNetworkState())
        {
            e_state = STATE_AT_CHECK;
        }
        else
        {
            e_state = STATE_DATA_CNX;
        }
        break;

    case STATE_DATA_CNX:
        if (false == processDataConnState())
        {
            e_state = STATE_NET_STATUS;
        }
        break;

    case STATE_DATA_SESSIONS:
        e_state = STATE_NET_STATUS;
        if (true == pdp::process_sessions())
        {
            //
        }
        else
        {
            if (++s_status.u8_check_retry > 4) {
                LOGW("data session process error");
                reset(false);
                delayms(10*1000);
            }
            delayms(1000);
        }
        break;

    default:
        e_state = STATE_INIT;
        break;
    }

    if ((millis() - s_reset.ms_hour) > (60 * 60 * 1000UL))
    {
        s_reset.ms_hour = millis();
        s_reset.u8_count = 0;
    }

    mdev.handle_events(10);
}

void reset(bool b_full_reset)
{
    e_state = STATE_INIT;

    if (s_reset.u8_count >= K_MODEM_MANAGER_MAX_RESET_PER_HOUR)
    {
        // try again after 1 hour
    }
    else if (0 == s_reset.ms_start)
    {
        // do power supply reset on the last attempt
        if ((K_MODEM_MANAGER_MAX_RESET_PER_HOUR - 1) == s_reset.u8_count)
        {
            // try poweroff the modem before cutting the supply
            (void)sendAT(&mdev, K_MODEM_STR_CMD_POWER_DOWN "=%d", 2 /*fast shutdown*/);
            delayms(200); // fast shutdown time is less than 100 ms

            LOGW("power reset");
            b_full_reset = true;
            modem_vcc_rst_pin(false);
            delayms(3*1000UL);
            modem_vcc_rst_pin(true);
            delayms(100); // at least 30ms
        }

        s_reset.u8_count++;
        s_reset.ms_start = millis();
        LOGD("modem reset %u", s_reset.u8_count);
        mdev.reset(b_full_reset);

        mdev.s_net_status.ms_last_update        = 0;
        mdev.s_net_status.s_cell.n_cell_type    = mdev.CELL_UNKNOWN;
        mdev.s_net_status.s_signal_quality.ui8_signal_fail_cntr = 0;
        mdev.s_net_status.s_registration.n_stat = mdev.REG_STAT_UNKNOWN;

        mdev.s_cnx_cfg.num_connect_fail         = 0;
        mdev.s_cnx_cfg.num_fail_activate        = 0;
        mdev.s_cnx_cfg.num_fail_apnverify       = 0;
        mdev.s_cnx_cfg.n_pdp_status             = mdev.PDP_STATE_UNKNOWN;
        mdev.s_cnx_cfg.n_ctx_state              = mdev.CTX_UNKNOWN_STATE;
        mdev.s_cnx_cfg.idx_apn = 0;

        memset(&mdev.s_info, 0, sizeof(mdev.s_info));
        memset(&mdev.s_net_status.s_operator, 0, sizeof(mdev.s_net_status.s_operator));

        setCommsFlag(SIM, false);
        setCommsFlag(DTLS_COMMS, false);
        setCommsFlag(CLOUD_CONN, false);
        setCommsFlag(MCU_TO_MODEM_COMMS, false);
        setCommsFlag(4G_CONN, false);
        setCommsFlag(NETWORK_OPERATOR, false);
        _4g_state = 0;
        g_b_dtls_ok = false;
        g_b_mcu_to_modem_ok = false;
        g_b_sim_ok = false;
        g_b_udp_ok = false;
    }
    else if (mdev.is_powerdown())
    {
        LOGW("modem powered down");
        delayms(2*1000UL);
        s_reset.ms_start = 0;
    }
    else if ((millis() - s_reset.ms_start) > (15 * 1000UL))
    {
        LOGW("ready timeout");
        s_reset.ms_start = 0;
    }
    else if (mdev.is_ready())
    {
        LOGD("modem ready");
        s_reset.ms_start = 0;
        e_state          = STATE_AT_CHECK;
        e_config_state   = CONFIG_STATE_INIT_CONFIG;
        e_network_state  = NETWORK_STATE_GET_SIGNAL_QUALITY;
        e_pdpctx_state   = PDPCTX_STATE_LOOKUP_APN;
    }
}

bool has_data_connection(void)
{
    return (mdev.CTX_ACTIVATED == mdev.s_cnx_cfg.n_ctx_state);
}

bool get_connection_state(bool *pb_state, const char **p_address, const char **p_apn)
{
    bool b_result;
    b_result = false;

    if (LOCK_INFO_ACCESS())
    {
        *pb_state = has_data_connection();
        if (NULL != p_address) {
            *p_address = mdev.s_cnx_cfg.ac_address;
        }
        if (NULL != p_apn) {
            *p_apn = mdev.s_cnx_cfg.ac_apn;
        }
        UNLOCK_INFO_ACCESS();
        b_result = true;
    }

    return b_result;
}

/*
 * Private Functions
 */
static bool processConfigState(void)
{
    bool b_result = false;

    switch (e_config_state)
    {
    case CONFIG_STATE_INIT_CONFIG:
        if (!sendAT(&mdev, K_MODEM_STR_CMD_INIT_CONFIG)) {
            //
        } else if (mdev.waitOK() < 1) {
            LOGW("init-config error");
        } else {
            //LOGD("init-config ok");
            e_config_state = CONFIG_STATE_ENABLE_CMEE;
            b_result = true;
        }
        break;

    case CONFIG_STATE_ENABLE_CMEE:
        if (!sendAT(&mdev, K_MODEM_STR_CMD_SET_CMEE "=%d", 1)) {
            //
        } else if (mdev.waitOK() < 1) {
            LOGW("enable-cmee error");
        } else {
            //LOGD("enable-cmee ok");
            e_config_state = CONFIG_STATE_ENABLE_CREG_URC;
            b_result = true;
        }
        break;

    case CONFIG_STATE_ENABLE_CREG_URC:
#if 0
        if (!sendAT(&mdev, K_MODEM_STR_CMD_NETWORK_REGISTRATION "=%d", 1 /*registration only*/)) {
            //
        } else if (mdev.waitOK() < 1) {
            LOGW("enable-creg-urc error");
        } else
#endif
        if (!sendAT(&mdev, K_MODEM_STR_CMD_EPS_REGISTRATION "=%d", 2 /*registration + location*/)) {
            //
        } else if (mdev.waitOK() < 1) {
            LOGW("enable-cereg-urc error");
        } else {
            //LOGD("enable-creg-urc ok");
            e_config_state = CONFIG_STATE_INIT_CONFIG;
            e_state = STATE_MODEM_INFO;
            b_result = true;
        }
        break;
    }

    return b_result;
}

static inline void _updateRssiStats(int16_t si16_rssi, int16_t si16_rsrp, int16_t si16_rsrq)
{
    if (si16_rssi < mdev.s_net_status.s_signal_quality.si16_rssi_min) {
        mdev.s_net_status.s_signal_quality.si16_rssi_min = si16_rssi;
    }

    if (si16_rssi > mdev.s_net_status.s_signal_quality.si16_rssi_max) {
        mdev.s_net_status.s_signal_quality.si16_rssi_max = si16_rssi;
    }

    mdev.s_net_status.s_signal_quality.si16_rssi_acc += si16_rssi;
    mdev.s_net_status.s_signal_quality.ui16_rssi_sample_ctr++;

    mdev.s_net_status.s_signal_quality.n_rsrp = si16_rsrp;
    mdev.s_net_status.s_signal_quality.n_rsrq = si16_rsrq;
}

static bool processNetworkState(void)
{
    static uint32_t ms_last_fail = 0;
    static uint32_t ms_last_read_signal = 0;
    bool b_result = false;

    if (millis() - ms_last_fail < 10000)
    {
        //LOGD("ticks %lu", millis());
        return true;
    }

    b_result = true;
    switch (e_network_state)
    {
    case NETWORK_STATE_GET_SIGNAL_QUALITY:
        if ((mdev.s_net_status.s_signal_quality.ui16_rssi_sample_ctr > 0) &&
            (millis() - ms_last_read_signal < MODEM_NET_STAT_UPDATE_INTERVAL))
        {
            // skip
        }
        else if (true == (b_result = mdev.get_signal_quality()))
        {
            if ((mdev.REG_STAT_UNKNOWN == mdev.s_net_status.s_registration.n_stat) ||
                (mdev.REG_STAT_NOT_REGISTERED == mdev.s_net_status.s_registration.n_stat))
            {
                e_network_state = NETWORK_STATE_GET_REGISTRATION;
            }
            else if ((0 != mdev.s_net_status.s_registration.n_urc) ||
                     ('\0' == mdev.s_net_status.s_operator.ac_name[0]))
            {
                mdev.s_net_status.s_registration.n_urc = 0;
                e_network_state = NETWORK_STATE_GET_OPERATOR;
            }
            else
            {
                ms_last_read_signal = millis();
                if (!mdev.in_serving_cell() || mdev.is_searching_operator())
                {
                    mdev.s_net_status.s_signal_quality.ui8_signal_fail_cntr++;
                    if ( mdev.s_net_status.s_signal_quality.ui8_signal_fail_cntr > 19 )
                    {
                        LOGW("max number of invalid rssi / searching operator");
                        reset(false);
                    }
                }
                else
                {
                    mdev.s_net_status.s_signal_quality.ui8_signal_fail_cntr = 0;
                }
    #if 0
                _updateRssiStats(mdev.s_net_status.s_signal_quality.n_rssi, mdev.s_net_status.s_signal_quality.n_rsrp, mdev.s_net_status.s_signal_quality.n_rsrq);
    #else // use the values from cell info instead
                e_network_state = NETWORK_STATE_GET_CELL_INFO;
    #endif
            }
        }
        else
        {
            ms_last_fail = millis();
            mdev.s_net_status.s_signal_quality.ui8_signal_fail_cntr++;
            if (  mdev.s_net_status.s_signal_quality.ui8_signal_fail_cntr > 19 )
            {
                LOGW("max number of rssi error ");
                reset(false);
            }
        }
        break;

    case NETWORK_STATE_GET_REGISTRATION:
        if (false == mdev.get_network_registration())
        {
            // if not yet registered to network, re-check signal quality
            e_network_state = NETWORK_STATE_GET_SIGNAL_QUALITY;
            ms_last_fail = millis();
        }
        else
        {
            e_network_state = NETWORK_STATE_GET_OPERATOR;
        }
        break;

    case NETWORK_STATE_GET_OPERATOR:
        if (true == mdev.get_operator_info(K_MODEM_OPERATOR_MODE, K_MODEM_OPERATOR_FORMAT))
        {
            setCommsFlag(NETWORK_OPERATOR, true);
            if(0 != strcmp(mdev.s_net_status.s_operator.ac_name, s_prev_net_status.s_operator.ac_name))
            {
                LOGD("operator change");
                s_status.b_oper_change = true;
            }
            else
            {
                s_status.b_oper_change = false;
            }
            (void)strncpy(s_prev_net_status.s_operator.ac_name, mdev.s_net_status.s_operator.ac_name, sizeof(mdev.s_net_status.s_operator.ac_name));
        }
        else
        {
            ms_last_fail = millis();
        }
        e_network_state = NETWORK_STATE_GET_CELL_INFO;
        break;

    case NETWORK_STATE_GET_CELL_INFO:
        // will retrieve both network band & cell info
        if (true == mdev.get_cell_info())
        {
            if (0 != strcmp(mdev.s_net_status.s_rfband.ac_band, s_prev_net_status.s_rfband.ac_band))
            {
                //LOGD("rf band change");
                (void)strncpy(s_prev_net_status.s_rfband.ac_band, mdev.s_net_status.s_rfband.ac_band, sizeof(mdev.s_net_status.s_rfband.ac_band));
                s_status.b_band_change = true;
            }
            else
            {
                s_status.b_band_change = false;
            }

            mdev.s_net_status.ms_last_update = millis();

            _updateRssiStats(mdev.s_net_status.s_signal_quality.n_rssi, mdev.s_net_status.s_signal_quality.n_rsrp, mdev.s_net_status.s_signal_quality.n_rsrq);
        }
        else
        {
      #if 0 // test only
            LOGW("proceed with pdp activation");
            mdev.s_net_status.s_cell.n_cell_type = mdev.CELL_LTE_SERVING;
      #endif
            // ignore error
        }
        e_network_state = NETWORK_STATE_GET_SIGNAL_QUALITY;
        break;

    default:
        b_result = false;
        e_network_state = NETWORK_STATE_GET_SIGNAL_QUALITY;
        break;
    }

    return b_result;
}

static bool processDataConnState(void)
{
    static char     ac_apn1[MODEM_APN_MAX_STR_LENGTH] = {0, };
    static char     ac_apn2[MODEM_APN_MAX_STR_LENGTH] = {0, };
    static uint32_t ms_activate_start = 0;
    static int      id_ctx = 0;
    static int      num_apns = 0;

    bool        b_result = false;

    //LOGD("%s(state=%d)", __func__, e_pdpctx_state);

    switch (e_pdpctx_state)
    {
    case PDPCTX_STATE_LOOKUP_APN:
        if (strlen(mdev.s_info.ac_imsi) >= 5)
        {
            memset(ac_apn1, 0, sizeof(ac_apn1));
            memset(ac_apn2, 0, sizeof(ac_apn2));

            (void)mdev.apn_lookup(mdev.s_info.ac_imsi, ac_apn1, mdev.PDP_CTX_ID_DEFAULT_APN - 1);
            if (false == (s_status.b_custom_apn_used = processCheckCustomApn(ac_apn2)))
            {
                (void)mdev.apn_lookup(mdev.s_info.ac_imsi, ac_apn2, mdev.PDP_CTX_ID_CUSTOM_APN - 1);
            }

            num_apns = (0 == strncmp(ac_apn1, ac_apn2, sizeof(ac_apn1))) ? 1 : 2;

            e_pdpctx_state = PDPCTX_STATE_VERIFY_APN;
            b_result = true;

        }
        break;

    case PDPCTX_STATE_VERIFY_APN:
        if ((false == mdev.get_connection_config(mdev.PDP_CTX_ID_DEFAULT_APN)) ||
            (0 != strncmp(mdev.s_cnx_cfg.ac_apn, ac_apn1, sizeof(ac_apn1))))
        {
            LOGW("verify apn1 failed \"%s\" (\"%s\")", mdev.s_cnx_cfg.ac_apn, ac_apn1);
            mdev.s_cnx_cfg.num_fail_apnverify++;
            e_pdpctx_state = PDPCTX_STATE_SET_APN;
        }
        else if (1 == num_apns)
        {
            LOGD("apn: \"%s\"", ac_apn1);
            mdev.s_cnx_cfg.num_fail_apnverify = 0;
            id_ctx = mdev.PDP_CTX_ID_DEFAULT_APN;
        }
        else if ((false == mdev.get_connection_config(mdev.PDP_CTX_ID_CUSTOM_APN)) ||
                 (0 != strncmp(mdev.s_cnx_cfg.ac_apn, ac_apn2, sizeof(ac_apn2))))
        {
            LOGW("verify apn2 failed \"%s\" (\"%s\")", mdev.s_cnx_cfg.ac_apn, ac_apn2);
            mdev.s_cnx_cfg.num_fail_apnverify++;
            e_pdpctx_state = PDPCTX_STATE_SET_APN;
        }
        else
        {
            LOGD("apn1: \"%s\"", ac_apn1);
            LOGD("apn2: \"%s\"", ac_apn2);
            mdev.s_cnx_cfg.num_fail_apnverify = 0;
            if (++id_ctx > mdev.PDP_CTX_ID_MAX_ID) {
                id_ctx = mdev.PDP_CTX_ID_DEFAULT_APN;
            }
        }

        if (0 == mdev.s_cnx_cfg.num_fail_apnverify)
        {
            e_pdpctx_state = PDPCTX_ACTIVATE;
        }
        else if (mdev.s_cnx_cfg.num_fail_apnverify > MODEM_PDP_MAX_FAIL_ATTEMPTS)
        {
            e_state = STATE_INIT; // will reset modem
        }
        else // try again
        {
            delayms(3000UL);
        }
        b_result = true;
        break;

    case PDPCTX_STATE_SET_APN:
        mdev.deactivate_pdp(mdev.PDP_CTX_ID_DEFAULT_APN);
        mdev.set_connection_config(mdev.PDP_CTX_ID_DEFAULT_APN, ac_apn1);
        if (num_apns > 1)
        {
            mdev.deactivate_pdp(mdev.PDP_CTX_ID_CUSTOM_APN);
            mdev.set_connection_config(mdev.PDP_CTX_ID_CUSTOM_APN, ac_apn2);
        }

        e_state         = STATE_NET_STATUS;
        e_pdpctx_state  = PDPCTX_STATE_LOOKUP_APN;
        b_result        = true;
        break;

    case PDPCTX_ACTIVATE:
        // if registered to the network or aleast camping on a serving cell
        if (mdev.is_network_registered() || mdev.in_serving_cell())
        {
            if (true == mdev.activate_pdp((CellularModem::pdp_ctx_et)id_ctx))
            {
                LOGD("PDP %d activate ...", id_ctx);
                ms_activate_start   = millis();
                e_pdpctx_state      = PDPCTX_ACTIVATING;
                b_result            = true;
            }
        }
        else // [re]check if there's already a serving cell
        {
            mdev.get_cell_info();
            delayms(3*1000UL);
        }
        break;

    case PDPCTX_ACTIVATING:
        b_result = true;
        if (ATModem::AT_RESPONSE_OK == mdev.cmd_state())
        {
            LOGD("pdp activate ok");
            mdev.s_cnx_cfg.num_fail_activate = 0;
            e_pdpctx_state = PDPCTX_ACTIVATED;
        }
        else if (ATModem::AT_RESPONSE_ERR == mdev.cmd_state())
        {
            mdev.s_cnx_cfg.num_fail_activate++;
            LOGW("PDP %d activate failed #%u", id_ctx, mdev.s_cnx_cfg.num_fail_activate);
            mdev.show_pdp_error();

            if (mdev.s_cnx_cfg.num_fail_activate > MODEM_PDP_MAX_FAIL_ATTEMPTS)
            {
                e_state = STATE_INIT; // will reset modem
            }
            else // try again
            {
                delayms(3000UL);
                e_pdpctx_state = PDPCTX_STATE_LOOKUP_APN;
                b_result = false;
            }
        }
        else if (millis() - ms_activate_start > MODEM_PDP_ACTIVATE_TIMEOUT)
        {
            LOGW("PDP %d activate timeout", id_ctx);
            e_state = STATE_INIT; // will reset modem
        }
        break;

    case PDPCTX_ACTIVATED:
        if (true == mdev.get_active_connection())
        {
            (void)mdev.get_connection_config((CellularModem::pdp_ctx_et)mdev.s_cnx_cfg.id_ctx);
            setCommsFlag(4G_CONN, true);
            _4g_state = 1;
            LOGI("ctx=%d apn=%s ip=%s", mdev.s_cnx_cfg.id_ctx, mdev.s_cnx_cfg.ac_apn, mdev.s_cnx_cfg.ac_address);
            pdp::init_sessions();
            e_pdpctx_state = PDPCTX_STATE_READY;
            b_result = true;
        }
        break;

    case PDPCTX_STATE_READY:
        e_state = STATE_DATA_SESSIONS;
        b_result = true;
        break;

    default:
        e_pdpctx_state = PDPCTX_STATE_LOOKUP_APN;
        e_state = STATE_NET_STATUS;
        break;
    }

    return b_result;
}

static bool processCheckCustomApn(char *pc_apn_buff)
{
    LOGW("to do: %s", __func__);
    return false;
}

} // namespace modem::manager

} // namespace modem
