#include "general_info.h"
#include <stdint.h>

const char *fw_version;
char device_id[24];
char *ac_serialnumber = "--";
char *ac_imei         = "--";
char *ac_imsi         = "--";
char *ac_iccid        = "--";
char enmtr_version[24]   = "--";
int8_t wifi_rssi;
int8_t lora_rssi;
int8_t blufi_rssi;
int8_t _4g_rssi;
int8_t wifi_state; // 0 offline, 1 online, 2 idle
int8_t lora_state;
int8_t blufi_state;
int8_t _4g_state;
bool g_b_dtls_ok;
bool g_b_mcu_to_modem_ok;
bool g_b_sim_ok;
bool g_b_udp_ok;