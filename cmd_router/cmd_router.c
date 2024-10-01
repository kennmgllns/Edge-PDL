
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "esp_wifi.h"

#include "lwip/ip4_addr.h"
#if !IP_NAPT
#error "IP_NAPT must be defined"
#endif
#include "lwip/lwip_napt.h"

#include "router_globals.h"
#include "cmd_router.h"

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
#define WITH_TASKS_INFO 1
#endif

static const char *TAG = "cmd_router";

esp_err_t get_config_param_str(char* name, char** param)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        size_t len;
        if ( (err = nvs_get_str(nvs, name, NULL, &len)) == ESP_OK) {
            *param = (char *)malloc(len);
            err = nvs_get_str(nvs, name, *param, &len);
            ESP_LOGI(TAG, "%s %s", name, *param);
        } else {
            return err;
        }
        nvs_close(nvs);
    } else {
        return err;
    }
    return ESP_OK;
}

esp_err_t get_config_param_int(char* name, int* param)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        if ( (err = nvs_get_i32(nvs, name, (int32_t*)(param))) == ESP_OK) {
            ESP_LOGI(TAG, "%s %d", name, *param);
        } else {
            return err;
        }
        nvs_close(nvs);
    } else {
        return err;
    }
    return ESP_OK;
}

esp_err_t get_config_param_blob(char* name, uint8_t** blob, size_t blob_len)
{
    nvs_handle_t nvs;

    esp_err_t err = nvs_open(PARAM_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        size_t len;
        if ( (err = nvs_get_blob(nvs, name, NULL, &len)) == ESP_OK) {
            if (len != blob_len) {
                return ESP_ERR_NVS_INVALID_LENGTH;
            }
            *blob = (uint8_t *)malloc(len);
            err = nvs_get_blob(nvs, name, *blob, &len);
            ESP_LOGI(TAG, "%s: %d", name, len);
        } else {
            return err;
        }
        nvs_close(nvs);
    } else {
        return err;
    }
    return ESP_OK;
}