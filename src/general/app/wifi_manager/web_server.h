
#pragma once

#include <esp_http_server.h>

typedef struct {
    const char* serial_number;
    char device_id[24];
    const char* fw_version;
    const char* enmtr_version;
    const char* modem;
    const char* imei;
    const char* imsi;
    const char* ccid;
    const char* active_comms;
    char active_comms_status[24];
    char comms_link_status[12];
    char signal_strength[24];
    
} WebAppData; 

namespace web::server
{

bool start();
void stop();

} //namespace web::server
