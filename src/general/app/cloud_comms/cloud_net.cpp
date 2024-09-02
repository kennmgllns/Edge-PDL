#include <esp_wifi.h>
#include "global_defs.h"
#include "general_info.h"
#include "web_server.h"
#include "wifi_manager.h"
#include <esp_netif.h>
#include "lwip/inet.h"  // Include this header for IP4_ADDR
esp_netif_ip_info_t ipInfo;

namespace wifi
{
/*
 * Local Constants
 */
#define CONNECTED_BIT       BIT0
#define FAIL_BIT            BIT1

/*
 * Local Variables
 */
static EventGroupHandle_t   wifi_event_group; // rtos event group
static esp_netif_t         *sta_netif = NULL;
static esp_netif_t         *ap_netif = NULL;
static wifi_sta_list_t      s_sta_list;
static TickType_t           ms_soft_ap = 0; // soft-AP inactive timer
static state_et             e_state;

wifi_config_t               sta_config;
wifi_config_t               ap_config;
station_info_st             s_sta_info;

namespace manager
{
/*
 * Private Function Prototypes
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static bool reconnect(void);

/*
 * Public Functions
 */
bool init()
{
    LOGI("WM _INIT_");
    LOGD("core %u: %s", xPortGetCoreID(), __PRETTY_FUNCTION__);

    wifi_init_config_t  init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    bool                b_status = false;

    memset(&sta_config, 0, sizeof(sta_config));
    memset(&ap_config, 0, sizeof(ap_config));
    memset(&s_sta_info, 0, sizeof(s_sta_info));

    e_state = STATE_INIT;

    if (NULL == (wifi_event_group = xEventGroupCreate()))
    {
        LOGW("xEventGroupCreate() failed");
    }
    else if (ESP_OK != esp_netif_init())
    {
        LOGW("netif_init() failed");
    }
    else if (ESP_OK != esp_event_loop_create_default())
    {
        LOGW("event_loop_create_default() failed");
    }
    else if (NULL == (sta_netif = esp_netif_create_default_wifi_sta()))
    {
        LOGW("netif_create_default_wifi_sta() failed");
    }
    else if (NULL == (ap_netif = esp_netif_create_default_wifi_ap()))
    {
        LOGW("netif_create_default_wifi_sta() failed");
    }
    else if (ESP_OK != esp_wifi_init(&init_cfg))
    {
        LOGW("wifi_init() failed");
    }
#if 0
    else if (ESP_OK != esp_wifi_set_storage(WIFI_STORAGE_RAM)) // disable persistent
    {
        LOGW("wifi_set_storage(ram) failed");
    }
#else
    else if (ESP_OK != esp_wifi_get_config(WIFI_IF_STA, &sta_config))
    {
        LOGW("failed to read station config");
    }
    else if (ESP_OK != esp_wifi_get_config(WIFI_IF_AP, &ap_config))
    {
        LOGW("failed to read AP config");
    }
#endif
    else if (ESP_OK != esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL))
    {
        LOGW("register wifi event handler failed");
    }
    else if (ESP_OK != esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL))
    {
        LOGW("register ip event handler failed");
    }
    else if (ESP_OK != blufi::controller_init())
    {
        //
    }
    else if (ESP_OK != blufi::host_and_cb_init())
    {
        //
    }
    else if (false == web::server::start())
    {
        LOGW("web_server failed");
    }
    else
{
// #if 0
    // Initialize TCP/IP
    esp_netif_init();

    // Check if sta_netif already exists and destroy if it does
    if (sta_netif != NULL) {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }

    // Create the default WIFI station
    sta_netif = esp_netif_create_default_wifi_sta();

    // Check if sta_netif is created
    if (sta_netif == NULL) {
        LOGW("Failed to create default Wi-Fi STA interface");
        return false;
    }

    // Disable DHCP client
    esp_netif_dhcpc_stop(sta_netif);

    // Set a static IP address
    IP4_ADDR(&ipInfo.ip, 192, 168, 0, 200);   // Set static IP
    IP4_ADDR(&ipInfo.gw, 192, 168, 0, 1);     // Set gateway
    IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0); // Set netmask

    // Apply the static IP configuration
    esp_err_t result = esp_netif_set_ip_info(sta_netif, &ipInfo);
    if (result != ESP_OK) {
        LOGW("Failed to set static IP, error code: %d", result);
    }
// #endif
    // Start the Wi-Fi
    if (ESP_OK != esp_wifi_start())
    {
        LOGW("wifi_start() failed");
        wifi_state = 0;
        wifi_rssi = 0;
    }
    else
    {
        LOGI("ap: %s", ap_config.ap.ssid);
        LOGI("sta: %s", sta_config.sta.ssid);
        b_status = true;
    }
}



    return b_status;
}

// Rest of your existing code...


void cycle()
{
    static TickType_t   ms_start = 0;
    static bool         b_warned = false;


        
    wifi_mode_t mode;
    // LOGW("e state: %d",e_state);
    switch (e_state)
    {
    case STATE_INIT:
        if (ESP_OK != esp_wifi_set_mode(WIFI_MODE_STA))
        {
            LOGW("wifi_set_mode(STA) failed");
            wifi_state = 0;
            wifi_rssi = 0;
        }
        else if (ESP_OK != esp_wifi_start())
        {
            LOGW("wifi_start() failed");
            wifi_state = 0;
            wifi_rssi = 0;
        }
        else
        {
            s_sta_info.u8_retry = 0;
            ms_start = millis();
            e_state = STATE_CONNECT;
        }
        break;

    case STATE_CONNECT:
        if (0 != (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT))
        {
            LOGD("connected");
            e_state = STATE_CHECK;
            wifi_state = 1;
        }
        else if (millis() - ms_start > K_WIFI_CONNECTION_TIMEOUT_MS)
        {
            LOGW("connect timeout");
            (void)esp_wifi_stop();
            //e_state = STATE_INIT;
            wifi_state = 0;
            wifi_rssi = 0;
            e_state = STATE_CHECK;
        }
        break;

    case STATE_CHECK:
        esp_wifi_get_mode(&mode);
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            wifi_rssi = ap_info.rssi;
            
        }
        if ((WIFI_MODE_AP == mode) || (WIFI_MODE_APSTA == mode))
        {
            LOGD("soft-AP %d client", softap_connections_count());
            if (softap_connections_count() > 0)
            {
                ms_soft_ap = millis();
            }
            else if (millis() - ms_soft_ap > K_WIFI_SOFTAP_INACTIVE_MS)
            {
                LOGW("soft-ap inactive");
                (void)esp_wifi_stop();
                e_state = STATE_INIT;
            }
        }
        else if (s_sta_info.b_connected)
        {
            if (b_warned) {
                LOGI("reconnected");
                b_warned = false;
            }
        }
        else
        {
            if (!b_warned) {
                LOGW("disconnected, retrying ...");
                b_warned = true;
            }
            (void)esp_wifi_stop();
            e_state = STATE_INIT;
        }
        break;

    default:
        e_state = STATE_INIT;
        break;
    }
}

int softap_connections_count(void)
{
    return ESP_OK == esp_wifi_ap_get_sta_list(&s_sta_list) ? s_sta_list.num : 0;
}

/*
 * Private Functions
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    esp_blufi_ap_record_t  *blufi_ap_list = NULL;
    wifi_ap_record_t       *ap_list = NULL;
    uint16_t                ap_count = 0;

    //LOGD("%s (id=%ld)", event_base, event_id);
    LOGW("w event handler event_id: %d",event_id);
    switch (event_id)
    {
    case WIFI_EVENT_SCAN_DONE:
        if ((ESP_OK != esp_wifi_scan_get_ap_num(&ap_count)) || (0 == ap_count))
        {
            LOGD("no AP found");
        }
        else if (NULL == (ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count)))
        {
            LOGE("malloc error");
        }
        else if (ESP_OK != esp_wifi_scan_get_ap_records(&ap_count, ap_list))
        {
            LOGW("get ap records failed");
        }
        else if (NULL == (blufi_ap_list = (esp_blufi_ap_record_t *)malloc(sizeof(esp_blufi_ap_record_t) * ap_count)))
        {
            LOGE("malloc error");
        }
        else
        {
            LOGD("found %u APs:", ap_count);
            for (uint16_t i = 0; i < ap_count; ++i)
            {
                blufi_ap_list[i].rssi = ap_list[i].rssi;
                //memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
                snprintf((char*)blufi_ap_list[i].ssid, sizeof(blufi_ap_list[i].ssid) - 1,
                        "%s (" MACSTR ")", ap_list[i].ssid, MAC2STR(ap_list[i].bssid));
                printf("%5d. " MACSTR " - %s (ch%d %ddBm)\r\n", i+1, MAC2STR(ap_list[i].bssid),
                        ap_list[i].ssid, ap_list[i].primary, ap_list[i].rssi);
            }

            if (s_sta_info.b_ble_connected) {
                esp_blufi_send_wifi_list(ap_count, blufi_ap_list);
            }
        }

        esp_wifi_scan_stop();
        if (ap_list) { free(ap_list); }
        if (blufi_ap_list) { free(blufi_ap_list); }
        break;

    case WIFI_EVENT_STA_START:
        LOGD("sta start");
        s_sta_info.u8_retry = 0;
        s_sta_info.b_connecting = (esp_wifi_connect() == ESP_OK);
        blufi::record_wifi_conn_info();
        break;

    case WIFI_EVENT_STA_CONNECTED:
        {
            LOGD("sta connected");
            wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t*) event_data;
            memcpy(s_sta_info.au8_bssid, event->bssid, sizeof(s_sta_info.au8_bssid));
            memcpy(s_sta_info.au8_ssid, event->ssid, sizeof(s_sta_info.au8_ssid));
            s_sta_info.len_ssid     = event->ssid_len;
            s_sta_info.b_connected  = true;
            s_sta_info.b_connecting = false;
            s_sta_info.u8_retry     = 0;
        }
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        //LOGW("sta disconnected");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        if (false == reconnect())
        {
            wifi_event_sta_disconnected_t *disconnected_event = (wifi_event_sta_disconnected_t*) event_data;
            blufi::record_wifi_conn_info(disconnected_event->rssi, disconnected_event->reason);

            LOGW("connect to AP failed");
            xEventGroupSetBits(wifi_event_group, FAIL_BIT);

            s_sta_info.b_connecting = false;
            s_sta_info.u8_retry     = 0;
        }

        memset(s_sta_info.au8_ssid, 0, sizeof(s_sta_info.au8_ssid));
        memset(s_sta_info.au8_bssid, 0, sizeof(s_sta_info.au8_bssid));
        s_sta_info.len_ssid     = 0;
        s_sta_info.b_connected  = false;
        s_sta_info.b_got_ip     = false;
        break;

    case WIFI_EVENT_AP_START:
        ms_soft_ap = millis();
        if (s_sta_info.b_ble_connected) {
            blufi::send_connection_report();
        }
        break;

    case WIFI_EVENT_STA_STOP:
        LOGD("ignore EVENT_STA_STOP");
        break;

    case WIFI_EVENT_AP_STOP:
        LOGD("ignore EVENT_AP_STOP");
        break;

    case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            LOGD("station " MACSTR " join (aid=%d)", MAC2STR(event->mac), event->aid);
        }
        break;

    case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            LOGD("station " MACSTR " leave  (aid=%d)", MAC2STR(event->mac), event->aid);
        }
        break;

    case WIFI_EVENT_STA_BEACON_TIMEOUT:
        LOGW("sta beacon timeout");
        break;

    case WIFI_EVENT_HOME_CHANNEL_CHANGE:
        //LOGD("ignore EVENT_CHANNEL_CHANGE");
        break;

    default:
        LOGW("%s: not handled event %d", __func__, event_id);
        break;
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    //LOGD("%s (id=%ld)", event_base, event_id);

    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        {
            ip_event_got_ip_t *got_ip = (ip_event_got_ip_t*)event_data;
            LOGI("ip addr: " IPSTR, IP2STR(&got_ip->ip_info.ip));

            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            s_sta_info.b_got_ip = true;
        }

        if (s_sta_info.b_ble_connected)
        {
            esp_blufi_extra_info_t info;
            wifi_mode_t mode;

            esp_wifi_get_mode(&mode);

            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, s_sta_info.au8_bssid, sizeof(s_sta_info.au8_bssid));
            info.sta_bssid_set = true;
            info.sta_ssid = s_sta_info.au8_ssid;
            info.sta_ssid_len = s_sta_info.len_ssid;
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_connections_count(), &info);
        }
        break;

    case IP_EVENT_STA_LOST_IP:
        LOGW("lost ip");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        s_sta_info.b_connected  = false;
        s_sta_info.b_got_ip     = false;
        break;

    default:
        LOGW("%s: not handled event %d", __func__, event_id);
        break;
    }

}

static bool reconnect(void)
{
    bool b_status = false;

    if (s_sta_info.u8_retry++ < K_WIFI_CONNECTION_MAXIMUM_RETRY)
    {
        LOGD("reconnect %u/%u ...", s_sta_info.u8_retry, K_WIFI_CONNECTION_MAXIMUM_RETRY);
        s_sta_info.b_connecting = (esp_wifi_connect() == ESP_OK);
        blufi::record_wifi_conn_info();
        b_status = true;
    }

    return b_status;
}

} // end namespace wifi::manager

} // end namespace wifi
