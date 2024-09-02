
#pragma once

#include <esp_bt.h>
#include <esp_blufi.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>


namespace wifi
{

namespace blufi
{

typedef enum {
    SEC_TYPE_DH_PARAM_LEN   = 0x00,
    SEC_TYPE_DH_PARAM_DATA  = 0x01,
    SEC_TYPE_DH_P           = 0x02,
    SEC_TYPE_DH_G           = 0x03,
    SEC_TYPE_DH_PUBLIC      = 0x04
} sec_type_et;

/*
 * Public Function Prototypes
 */
esp_err_t controller_init();
void controller_deinit();

esp_err_t host_and_cb_init();
void record_wifi_conn_info(int rssi=-128/*invalid rssi*/, uint8_t reason=255/*invalid reason*/);
void send_connection_report();

} // namespace wifi::blufi

} // namespace wifi
