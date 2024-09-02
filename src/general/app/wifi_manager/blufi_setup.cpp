
#include <esp_crc.h>
#include <esp_random.h>
#include <esp_wifi.h>

#include <mbedtls/aes.h>
#include <mbedtls/dhm.h>
#include <mbedtls/md5.h>

#include "global_defs.h"
#include "general_info.h"
#include "wifi_manager.h"


namespace wifi
{

// from esp-idf/components/bt/common/btc/profile/esp/blufi/blufi_prf.c
extern "C" void btc_blufi_report_error(esp_blufi_error_state_t state);

// from wifi_manager.cpp
extern wifi_config_t    sta_config;
extern wifi_config_t    ap_config;
extern station_info_st  s_sta_info;


namespace blufi
{

/*
 * Local Variables
 */

static struct {
#define DH_SELF_PUB_KEY_LEN     128
#define DH_SELF_PUB_KEY_BIT_LEN (DH_SELF_PUB_KEY_LEN * 8)
    uint8_t                 self_public_key[DH_SELF_PUB_KEY_LEN];

#define SHARE_KEY_LEN       128
#define SHARE_KEY_BIT_LEN   (SHARE_KEY_LEN * 8)
    uint8_t                 share_key[SHARE_KEY_LEN];
    size_t                  share_len;

#define PSK_LEN             16
    uint8_t                 psk[PSK_LEN];
    uint8_t                *dh_param;
    int                     dh_param_len;
    uint8_t                 iv[16];
    mbedtls_dhm_context     dhm;
    mbedtls_aes_context     aes;
} s_security;

static esp_blufi_extra_info_t   s_conn_info;

/*
 * Private Function Prototypes
 */
static void event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
static void dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
static int aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
static int aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
static uint16_t crc_checksum(uint8_t iv8, uint8_t *data, int len);

static int randfunc(void *rng_state, unsigned char *output, size_t len);
static void security_init(void);
static void security_deinit(void);

static esp_blufi_callbacks_t callbacks = {
    .event_cb = event_callback,
    .negotiate_data_handler = dh_negotiate_data_handler,
    .encrypt_func = aes_encrypt,
    .decrypt_func = aes_decrypt,
    .checksum_func = crc_checksum,
};


/*
 * Public Functions
 */
esp_err_t controller_init()
{
    esp_bt_controller_config_t  bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t                   err = ESP_OK;
    blufi_state = 1;
    if (ESP_OK != (err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)))
    {
        LOGW("controller_mem_release() failed (err=%d)", err);
        blufi_state = 0;
    }
    else if (ESP_OK != (err = esp_bt_controller_init(&bt_cfg)))
    {
        LOGW("controller_init() failed (err=%d)", err);
        blufi_state = 0;
    }
    else if (ESP_OK != (err = esp_bt_controller_enable(ESP_BT_MODE_BLE)))
    {
        LOGW("controller_enable() failed (err=%d)", err);
        blufi_state = 0;
    }

    // err = esp_bluedroid_enable();
    // if (err != ESP_OK) {
    //     LOGE("Failed to enable bluedroid, err=%d", err);
    // }

    return err;
}

void controller_deinit()
{
    (void)esp_bt_controller_disable();
    (void)esp_bt_controller_deinit();
    blufi_state = 2;
}

esp_err_t host_and_cb_init()
{
    esp_err_t err = ESP_OK;

    if (ESP_OK != (err = esp_bluedroid_init()))
    {
        LOGW("blufi_host_init() failed (err=%d)", err);
    }
    else if (ESP_OK != (err = esp_bluedroid_enable()))
    {
        LOGW("bluedroid_enable() failed (err=%d)", err);
    }
    else if (ESP_OK != (err = esp_blufi_register_callbacks(&callbacks)))
    {
        LOGW("register_callbacks() failed (err=%d)", err);
    }
    else if (ESP_OK != (err = esp_blufi_profile_init()))
    {
        LOGW("profile_init() failed (err=%d)", err);
    }
    else if (ESP_OK != (err = esp_ble_gap_register_callback(esp_blufi_gap_event_handler)))
    {
        LOGW("ble_gap_register() failed (err=%d)", err);
    }
    else
    {
        LOGI("BD addr: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));
    }
    return err;
}

void record_wifi_conn_info(int rssi, uint8_t reason)
{
    memset(&s_conn_info, 0, sizeof(s_conn_info));
    if (s_sta_info.b_connecting)
    {
        s_conn_info.sta_max_conn_retry_set = true;
        s_conn_info.sta_max_conn_retry = K_WIFI_CONNECTION_MAXIMUM_RETRY;
    }
    else
    {
        s_conn_info.sta_conn_rssi_set = true;
        s_conn_info.sta_conn_rssi = rssi;
        s_conn_info.sta_conn_end_reason_set = true;
        s_conn_info.sta_conn_end_reason = reason;
    }
}

void esp_blufi_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{  LOGW("Connected device RSSI: %d dBm", param->read_rssi_cmpl.rssi);
    switch (event)
    {
        case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
            {
                if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                LOGD("Connected device RSSI: %d dBm", param->read_rssi_cmpl.rssi);
                } else {
                    LOGE("Failed to read RSSI, status=%d", param->read_rssi_cmpl.status);
                }
            }
            break;
        default:
            break;
        // Handle other GAP events as needed
    }
}

void send_connection_report()
{
    wifi_mode_t mode;

    esp_wifi_get_mode(&mode);
    if (s_sta_info.b_connected)
    {
        esp_blufi_extra_info_t info;
        memset(&info, 0, sizeof(esp_blufi_extra_info_t));
        memcpy(info.sta_bssid, s_sta_info.au8_bssid, 6);
        info.sta_bssid_set = true;
        info.sta_ssid = s_sta_info.au8_ssid;
        info.sta_ssid_len = s_sta_info.len_ssid;
        esp_blufi_send_wifi_conn_report(mode, s_sta_info.b_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, manager::softap_connections_count(), &info);
    }
    else if (s_sta_info.b_connecting)
    {
        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, manager::softap_connections_count(), &s_conn_info);
    }
    else
    {
        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, manager::softap_connections_count(), &s_conn_info);
    }
}

/*
 * Private Functions
 */
static void event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    esp_err_t err;
    //LOGD("%s(%d)", __func__, event);
    switch (event)
    {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        LOGD("blufi adv started");
        esp_blufi_adv_start();
        break;

    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        err = esp_wifi_set_mode(param->wifi_mode.op_mode);
        LOGD("set wifi mode %d (err=%d)", param->wifi_mode.op_mode, err);
        break;

    case ESP_BLUFI_EVENT_BLE_CONNECT:
        {
            LOGD("client connected");
            s_sta_info.b_ble_connected = true;
            esp_blufi_adv_stop();
            security_init();
            esp_err_t err = esp_ble_gap_read_rssi(param->connect.remote_bda);
            // if (err == ESP_OK) {
            //     // RSSI will be reported in a GAP event, which you need to handle separately.
            //     LOGD("RSSI read request sent, waiting for response...");
            // } else {
            //     LOGE("Failed to initiate RSSI read, err=%d", err);
            // }
        }
        break;

    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        LOGD("client disconnected");
        s_sta_info.b_ble_connected = false;
        security_deinit();
        esp_blufi_adv_start();
        break;

    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        LOGD("wifi [re]connect to AP");
        esp_wifi_disconnect();
        s_sta_info.u8_retry = 0;
        s_sta_info.b_connecting = (esp_wifi_connect() == ESP_OK);
        record_wifi_conn_info();
        break;

    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        esp_wifi_disconnect();
        break;

    case ESP_BLUFI_EVENT_GET_WIFI_STATUS:
        send_connection_report();
        break;

    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        LOGD("recv sta bssid " MACSTR " (err=%d)", MAC2STR(sta_config.sta.bssid), err);
        break;

    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        LOGD("recv sta ssid %s (err=%d)", sta_config.sta.ssid, err);
        break;

    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        LOGD("recv sta password %s (err=%d)", sta_config.sta.password, err);
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
        strncpy((char *)ap_config.ap.ssid, (char *)param->softap_ssid.ssid, param->softap_ssid.ssid_len);
        ap_config.ap.ssid[param->softap_ssid.ssid_len] = '\0';
        ap_config.ap.ssid_len = param->softap_ssid.ssid_len;
        err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        LOGD("recv softap ssid %s (len=%d err=%d)", ap_config.ap.ssid, ap_config.ap.ssid_len, err);
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
        strncpy((char *)ap_config.ap.password, (char *)param->softap_passwd.passwd, param->softap_passwd.passwd_len);
        ap_config.ap.password[param->softap_passwd.passwd_len] = '\0';
        err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        LOGD("recv softap password %s (len=%d err=%d)", ap_config.ap.password, param->softap_passwd.passwd_len, err);
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
        if (param->softap_max_conn_num.max_conn_num <= 4)
        {
            ap_config.ap.max_connection = param->softap_max_conn_num.max_conn_num;
            err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
            LOGD("recv softap max conn %d (err=%d)", ap_config.ap.max_connection, err);
        }
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
        if (param->softap_auth_mode.auth_mode < WIFI_AUTH_MAX)
        {
            ap_config.ap.authmode = param->softap_auth_mode.auth_mode;
            err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
            LOGD("recv softap auth mode %d (err=%d)", ap_config.ap.authmode, err);
        }
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
        if (param->softap_channel.channel <= 13)
        {
            ap_config.ap.channel = param->softap_channel.channel;
            err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
            LOGD("recv softap channel %d (err=%d)", ap_config.ap.channel, err);
        }
        break;

    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        esp_blufi_disconnect();
        break;

    case ESP_BLUFI_EVENT_GET_WIFI_LIST:
        LOGD("start wifi scan");
        if (ESP_OK != esp_wifi_scan_start(NULL, true)) {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;

    case ESP_BLUFI_EVENT_REPORT_ERROR:
        LOGE("blufi error %d", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;

    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        LOGD("recv custom data: %.*s", param->custom_data.data_len, param->custom_data.data);
        LOGB(param->custom_data.data, param->custom_data.data_len);
        break;

    default:
        LOGW("%s: not handled event %d", __func__, event);
        break;
    }
}

static void dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free)
{
    int     ret;
    uint8_t type = data[0];

    //LOGD("%s()", __func__);

    switch (type)
    {
    case SEC_TYPE_DH_PARAM_LEN:
        s_security.dh_param_len = ((data[1]<<8)|data[2]);
        LOGD("dh_param_len=%d", s_security.dh_param_len);
        if (s_security.dh_param) {
            free(s_security.dh_param);
        }
        s_security.dh_param = (uint8_t *)malloc(s_security.dh_param_len);
        if (s_security.dh_param == NULL) {
            btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
            LOGE("%s, malloc failed", __func__);
        }
        break;

    case SEC_TYPE_DH_PARAM_DATA:
        if (s_security.dh_param == NULL) {
            LOGE("%s, dh_param == NULL", __func__);
            btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
        } else {
            uint8_t *param = s_security.dh_param;
            memcpy(s_security.dh_param, &data[1], s_security.dh_param_len);
            if (0 != (ret = mbedtls_dhm_read_params(&s_security.dhm, &param, &param[s_security.dh_param_len])))
            {
                LOGE("%s read param failed %d", __func__, ret);
                btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
            }
            else
            {
                free(s_security.dh_param);
                s_security.dh_param = NULL;

                const int dhm_len = mbedtls_dhm_get_len(&s_security.dhm);
                if (0 != (ret = mbedtls_dhm_make_public(&s_security.dhm, dhm_len, s_security.self_public_key, dhm_len, randfunc, NULL)))
                {
                    LOGE("%s make public failed %d", __func__, ret);
                    btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                }
                else if (0 != (ret = mbedtls_dhm_calc_secret( &s_security.dhm, s_security.share_key, SHARE_KEY_BIT_LEN, &s_security.share_len, randfunc, NULL)))
                {
                    LOGE("%s mbedtls_dhm_calc_secret failed %d", __func__, ret);
                    btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                }
                else if (0 != (ret = mbedtls_md5(s_security.share_key, s_security.share_len, s_security.psk)))
                {
                    LOGE("%s mbedtls_md5 failed %d", __func__, ret);
                    btc_blufi_report_error(ESP_BLUFI_CALC_MD5_ERROR);
                }
                else
                {
                    LOGD("dh param data ok");
                    mbedtls_aes_setkey_enc(&s_security.aes, s_security.psk, 128);
                    /* alloc output data */
                    *output_data = &s_security.self_public_key[0];
                    *output_len = dhm_len;
                    *need_free = false;
                }
            }
        }
        break;

    default:
        LOGW("%s: not handled type %d", __func__, type);
        break;
    }
}

static int aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    //LOGD("%s()", __func__);

    size_t  iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0, s_security.iv, sizeof(s_security.iv));
    iv0[0] = iv8;   /* set iv8 as the iv0[0] */

    return (0 == mbedtls_aes_crypt_cfb128(&s_security.aes, MBEDTLS_AES_ENCRYPT, crypt_len,
                &iv_offset, iv0, crypt_data, crypt_data)) ? crypt_len : -1;
}

static int aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    //LOGD("%s()", __func__);

    size_t  iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0, s_security.iv, sizeof(s_security.iv));
    iv0[0] = iv8;   /* set iv8 as the iv0[0] */

    return (0 == mbedtls_aes_crypt_cfb128(&s_security.aes, MBEDTLS_AES_DECRYPT, crypt_len,
                &iv_offset, iv0, crypt_data, crypt_data)) ? crypt_len : -1;
}

static uint16_t crc_checksum(uint8_t iv8, uint8_t *data, int len)
{
    //LOGD("%s()", __func__);
    (void)iv8;
    return esp_crc16_be(0, data, len);
}

static int randfunc(void *rng_state, unsigned char *output, size_t len)
{
    esp_fill_random(output, len);
    return 0;
}

static void security_init(void)
{
    memset(&s_security, 0x0, sizeof(s_security));
    mbedtls_dhm_init(&s_security.dhm);
    mbedtls_aes_init(&s_security.aes);
}

static void security_deinit(void)
{
    if (s_security.dh_param){
        free(s_security.dh_param);
        s_security.dh_param = NULL;
    }

    mbedtls_dhm_free(&s_security.dhm);
    mbedtls_aes_free(&s_security.aes);
}

} // end namespace wifi::blufi

} // end namespace wifi
