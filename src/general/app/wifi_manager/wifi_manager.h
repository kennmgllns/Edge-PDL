
#pragma once

#include "blufi_setup.h"

namespace wifi
{

/*
 * Global Constants
 */

#define K_WIFI_CONNECTION_MAXIMUM_RETRY     (5)
#define K_WIFI_CONNECTION_TIMEOUT_MS        (30*1000UL)
#define K_WIFI_SOFTAP_INACTIVE_MS           (5*60*1000UL)


typedef enum
{
    STATE_INIT = 0,
    STATE_CONNECT,  // wait to be connected
    STATE_CHECK,    // loop
} state_et;

typedef struct {
    bool    b_connected;
    bool    b_connecting;
    bool    b_ble_connected;
    bool    b_got_ip;
    uint8_t au8_bssid[6];
    uint8_t au8_ssid[32];
    int     len_ssid;
    uint8_t u8_retry;
} station_info_st;

namespace manager
{

/*
 * Public Function Prototypes
 */
bool init();
void cycle();

int softap_connections_count(void);


} // namespace wifi::manager

} // namespace wifi
