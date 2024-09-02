
#include "global_defs.h"
#include "general_info.h"

static void hw_init(void)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if ((ESP_ERR_NVS_NO_FREE_PAGES == err) ||
        (ESP_ERR_NVS_NEW_VERSION_FOUND == err))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );


    // Initialize internal storage
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false
    };
    wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

    // check "src/general/partitions/<size>.csv"
    err = esp_vfs_fat_spiflash_mount_rw_wl("/mnt", "storage", &mount_config, &s_wl_handle);
    ESP_ERROR_CHECK( err );
}

static void sw_init(void)
{
    uint8_t                 au8_devid[K_DEV_ID_LEN];
    const esp_app_desc_t   *app_desc    = esp_app_get_description();
    const uint8_t          *app_sha     = app_desc->app_elf_sha256;
    const esp_partition_t  *partition   = esp_ota_get_running_partition();

    systemFlagsInit();

    get_device_id(au8_devid);

    printf("\r\n");
    printf("--- %s   : %s ---\r\n", partition->label, app_desc->project_name);
    printf("--- sw ver : %s ---\r\n", app_desc->version);
    printf("--- sw sha : %02x%02x%02x%02x%02x%02x ---\r\n", app_sha[0], app_sha[1], app_sha[2], app_sha[3], app_sha[4], app_sha[4]);
    printf("--- dev id : %02x%02x%02x%02x%02x%02x ---\r\n", au8_devid[0], au8_devid[1], au8_devid[2], au8_devid[3], au8_devid[4], au8_devid[5]);
    
    fw_version = app_desc->version;
    sprintf(device_id,"%02x%02x%02x%02x%02x%02x",au8_devid[0], au8_devid[1], au8_devid[2], au8_devid[3], au8_devid[4], au8_devid[5]);




    // to do's: read from NVS
    // hw ver
    // ser num
    // prod date
    // calib

    uint64_t storage_total = 0, storage_free = 0;
    if (ESP_OK == esp_vfs_fat_info("/mnt", &storage_total, &storage_free))
    {
        printf("--- storage: %lld / %lld ---\r\n", storage_total - storage_free, storage_total);
    }

    const char *rst_reason = "UNKNOWN";
    switch (esp_reset_reason())
    {
      #define RST_CASE(cause) case ESP_RST_ ## cause: rst_reason = #cause; break
        RST_CASE(POWERON);
        RST_CASE(EXT);
        RST_CASE(SW);
        RST_CASE(PANIC);
        RST_CASE(INT_WDT);
        RST_CASE(TASK_WDT);
        RST_CASE(WDT);
        RST_CASE(DEEPSLEEP);
        RST_CASE(BROWNOUT);
        RST_CASE(SDIO);
        default: break; // unknown
    }

    printf("--- reset  : %s ---\r\n\r\n", rst_reason);

}

void global_init(void)
{

    hw_init();

    sw_init();

}
