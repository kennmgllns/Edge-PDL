#include "dev_info.h"
#include <stdint.h>
const char *fw_version;
const char *active_comms = "4G,LoRa,WiFi,BlueFi,Ethernet";
char active_comms_status[24];
char comms_link_status[12];
char signal_strength_bufffer[48] = "";
char device_id[24];
char *ac_serialnumber = "--";
char *ac_imei         = "--";
char *ac_imsi         = "--";
char *ac_iccid        = "--";
char *ac_model        = "--";
char enmtr_version[24]   = "--";
int8_t wifi_rssi;
int8_t lora_rssi;
int8_t blufi_rssi;
int8_t _4g_rssi;
int8_t eth_speed;
int8_t wifi_state; // 0 offline, 1 online, 2 idle
int8_t lora_state;
int8_t blufi_state;
int8_t _4g_state;
int8_t eth_state = 0;
bool g_b_dtls_ok;
bool g_b_mcu_to_modem_ok;
bool g_b_sim_ok;
bool g_b_udp_ok;
int8_t measure_interval1 = 1;
int16_t measure_interval2 = 60;
int8_t measure_interval_option = 1; //1 interv 1, 2 - interv 2
char wifi_ip_str[14];
char eth_ip_str[14];