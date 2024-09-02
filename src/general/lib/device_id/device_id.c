
#include <stdbool.h>
#include <string.h>

#include <esp_mac.h>
#include "device_id.h"


/*
 * Local Variables
 */
static uint8_t  au8_dev_id[K_DEV_ID_LEN];  // 48-bit device_id
static uint8_t  au8_base_mac[8];


void get_device_id(uint8_t *pui8_devi_id)
{
    static bool b_initilized = false;

    if (false == b_initilized)
    {
        b_initilized = (ESP_OK == esp_efuse_mac_get_default(au8_base_mac));
        memcpy(au8_dev_id, au8_base_mac, sizeof(au8_dev_id)); // MAC-48 only
    }

    memcpy(pui8_devi_id, au8_dev_id, sizeof(au8_dev_id));
}
