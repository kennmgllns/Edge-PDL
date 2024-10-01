idf_component_register(
    SRCS
        "main.cpp"
        "global_defs.cpp"
        "dev_info.cpp"
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
        "general/app/nat_router/nat_router.cpp"
        "general/app/hw_interact_manager/hw_interact_manager.cpp"

        
        "general/app/nat_router/src/Module.cpp"
        "general/app/nat_router/src/modules/SX127x/SX1278.cpp"
        "general/app/nat_router/src/modules/SX127x/SX127x.cpp"
        "general/app/nat_router/src/protocols/PhysicalLayer/PhysicalLayer.cpp"

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

        # "general/lib/sx126x/radio.c"
        # "general/lib/sx126x/sx126x.c"
        # "general/lib/sx126x/sx126x-board.c"

        # "core/core_spi_master.c"

        # "app/lora_mesh/cloud_mesh.c"
        # "app/lora_mesh/mesh_cloud.c"
        # "app/lora_mesh/mesh_commands.c"
        # "app/lora_mesh/mesh_comms.c"
        # "app/lora_mesh/mesh_route.c"
        # "app/lora_mesh/mesh_update.c"

    INCLUDE_DIRS
        "."
        "app/heartbeat"
        "app/data_logging"
        "app/input_monitoring"
        "app/lora_mesh"

        "config"

        "general/app/cloud_comms"
        "general/app/enmtr_manager"
        "general/app/hw_interact_manager"
        "general/app/modem_manager"
        "general/app/wifi_manager" 
        "general/app/nat_router"
        "general/lib"
        
        "cmd_router"

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
        esp_eth
        fatfs
        json
        nvs_flash
        esp_wifi wpa_supplicant
        esp_http_client
        lwip
)
