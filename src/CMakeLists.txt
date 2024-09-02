idf_component_register(
    SRCS
        "main.cpp"
        "global_defs.cpp"
        "general_info.cpp"
        "app/heartbeat/heartbeat.cpp"
        "app/cloud_comms/cloud_commands.cpp"
        "app/data_logging/data_logging.cpp"
        "app/input_monitoring/input_monitoring.cpp"
        "app/lora_mesh/lora_mesh.cpp"

        "general/app/cloud_comms/cloud_comms.cpp"
        "general/app/cloud_comms/cloud_net.cpp"
        "general/app/cloud_comms/cloud_sync.cpp"
        "general/app/cloud_comms/cloud_event.cpp"
        "general/app/cloud_comms/cloud_monitor.cpp"
        "general/app/cloud_comms/cloud_status.cpp"
        "general/app/cloud_comms/cloud_update.cpp"
        "general/app/enmtr_manager/enmtr_manager.cpp"
        "general/app/modem_manager/modem_manager.cpp"
        "general/app/modem_manager/modem_pdp.cpp"
        "general/app/wifi_manager/blufi_setup.cpp"
        "general/app/wifi_manager/web_server.cpp"
        "general/app/wifi_manager/wifi_manager.cpp"

        "general/lib/coap/coap_client.c"
        "general/lib/crc/crc16.c"
        "general/lib/device_id/device_id.c"
        "general/lib/dtls/dtls_client.c"
        "general/lib/dtls/dtls_crypto.c"
        "general/lib/edge_payload/edge_payload.c"
        "general/lib/logprint/logprint.c"
        "general/lib/system_flags/system_flags.c"

        "general/lib/enmtr/enmtr.cpp"
        "general/lib/enmtr/msp430.cpp"

        "general/lib/modem/modem.cpp"
        "general/lib/modem/quectel.cpp"
        "general/lib/modem/quectel_pdp.cpp"

    INCLUDE_DIRS
        "."
        "app/heartbeat"
        "app/data_logging"
        "app/input_monitoring"
        "app/lora_mesh"

        "config"

        "general/app/cloud_comms"
        "general/app/enmtr_manager"
        "general/app/modem_manager"
        "general/app/wifi_manager"

        "general/lib"

    EMBED_FILES
        "res/web/index.html"
        "res/web/favicon.svg"
        "res/web/style.css"
        "res/web/script.js"

    REQUIRES
        app_update
        bt
        driver
        esp_http_server
        esp_partition
        esp-tls
        esp_wifi
        fatfs
        json
        nvs_flash
)
