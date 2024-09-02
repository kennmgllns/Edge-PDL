
#pragma once

#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <esp_vfs_fat.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "crc/crc16.h"
#include "device_id/device_id.h"
#include "logprint/logprint.h"
#include "system_flags/system_flags.h"


#define K_APP_VER           "00.02.0002"    // <major>.<minor>.<test>


// at 1ms tick
#define delayms(ms)         vTaskDelay((ms))
#define millis()            xTaskGetTickCount()


void global_init(void);
