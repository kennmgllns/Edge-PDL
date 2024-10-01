
#ifndef dev_info_H
#define dev_info_H

#include <stdint.h>

extern const char *active_comms;
extern char active_comms_status[24];
extern char comms_link_status[12];
extern char signal_strength_bufffer[48];
extern const char *fw_version;
extern char device_id[24];
extern char *ac_serialnumber;
extern char *ac_imei;
extern char *ac_imsi;
extern char *ac_iccid;
extern char *ac_model;
extern char enmtr_version[24];
extern int8_t wifi_rssi;
extern int8_t lora_rssi;
extern int8_t blufi_rssi;
extern int8_t _4g_rssi;
extern int8_t eth_speed;
extern int8_t wifi_state; // 0 offline, 1 online, 2 idle
extern int8_t lora_state;
extern int8_t blufi_state;
extern int8_t _4g_state;
extern int8_t eth_state;
extern bool g_b_dtls_ok;
extern bool g_b_mcu_to_modem_ok;
extern bool g_b_sim_ok;
extern bool g_b_udp_ok;
extern int8_t measure_interval1;
extern int16_t measure_interval2;
extern int8_t measure_interval_option;
extern char wifi_ip_str[14];
extern char eth_ip_str[14];
#endif // dev_info_H
