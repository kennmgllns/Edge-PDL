
#ifndef GENERAL_INFO_H
#define GENERAL_INFO_H

#include <stdint.h>

extern const char *fw_version;
extern char device_id[24];
extern char *ac_serialnumber;
extern char *ac_imei;
extern char *ac_imsi;
extern char *ac_iccid;
extern char enmtr_version[24];
extern int8_t wifi_rssi;
extern int8_t lora_rssi;
extern int8_t blufi_rssi;
extern int8_t _4g_rssi;
extern int8_t wifi_state; // 0 offline, 1 online, 2 idle
extern int8_t lora_state;
extern int8_t blufi_state;
extern int8_t _4g_state;
extern bool b_dtls_ok;
extern bool b_mcu_to_modem_ok;
extern bool b_sim_ok;
extern bool b_udp_ok;

#endif // GENERAL_INFO_H
