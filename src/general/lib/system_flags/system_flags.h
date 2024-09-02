
#ifndef __SYSTEM_FLAGS_H__
#define __SYSTEM_FLAGS_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Global Constants
 */

typedef enum {
    SYSTEM_FLAGS_TIME_SYNC_STATUS               = (1<< 0),  // get/set TimeSyncStatus
    SYSTEM_FLAGS_RTC_ERROR_STATUS               = (1<< 1),  // get/set RtcErrorState
    SYSTEM_FLAGS_INTERNAL_ERROR_STATUS          = (1<< 2),  // get/set InternalErrorStatus

    SYSTEM_FLAGS_USER_CONFIG_STATUS             = (1<< 3),  // get/set UserConfigStatus
    SYSTEM_FLAGS_FACTORY_CONFIG_STATUS          = (1<< 4),  // get/set FactoryConfigStatus
    SYSTEM_FLAGS_MODBUS_PENDING_STATUS          = (1<< 5),  // get/set ModbusPendingStatus
    SYSTEM_FLAGS_CALIB_MODE_STATUS              = (1<< 6),  // get/set CalibModeStatus

    SYSTEM_FLAGS_CLOUD_RESET_REQUEST_STATUS     = (1<< 7),  // get/set CloudResetRequestStatus
    SYSTEM_FLAGS_MCU_RESET_REQUEST_STATUS       = (1<< 8),  // get/set McuResetRequestStatus
    SYSTEM_FLAGS_FACTORY_RESET_REQUEST_STATUS   = (1<< 9),  // get/set FactoryResetRequestStatus
    SYSTEM_FLAGS_MODEM_POWEROFF_REQUEST_STATUS  = (1<<10),  // get/set ModemPowerOffRequestStatus
    SYSTEM_FLAGS_FORCE_TIME_SYNC_STATUS         = (1<<11),  // get/set ForceTimeSyncStatus / ForceSyncTime
    SYSTEM_FLAGS_LOG_TX_STATUS                  = (1<<12),  // get/set logTxStatus

    SYSTEM_FLAGS_POWER_GOOD_STATUS              = (1<<13),  // get/set PowerGoodStatus
    SYSTEM_FLAGS_TRUE_POWER_OUTAGE_STATUS       = (1<<14),  // get/set TruePowerOutageStatus
    SYSTEM_FLAGS_EVENT_OFFLINE_STATUS           = (1<<15),  // get/set EventOffline

    SYSTEM_FLAGS_USER_LOGIN_STATUS              = (1<<16),  // get/set UserLoginStatus
    SYSTEM_FLAGS_LOW_POWER_STATUS               = (1<<17),  // get/set LowPowerStatus
    SYSTEM_FLAGS_BOOTLOAD_STATUS                = (1<<18),  // get/set BootloadStatus

} system_flags_masks_et; // system flags bit masks


/* extended system flags */
typedef enum {
    // cloud comms
    COMMS_FLAGS_CLOUD_CONN_STATUS               = (1<< 0),  // get/set CloudConnStatus
    COMMS_FLAGS_CLOUD_COMMS_FAULT_STATUS        = (1<< 1),  // get/set CloudCommsFault

    // internet comms
    COMMS_FLAGS_DTLS_COMMS_STATUS               = (1<< 2),  // get/set DtlsCommsStatus
    COMMS_FLAGS_DNS_STATUS                      = (1<< 3),  // get/set DnsStatus
    COMMS_FLAGS_PING_STATUS                     = (1<< 4),  // get/set PingStatus

    // Wi-Fi comms
    COMMS_FLAGS_WIFI_CONN_STATUS                = (1<< 5),  // get/set WifiConnStatus
    COMMS_FLAGS_WIFI_COMMS_STATUS               = (1<< 6),  // get/set WifiCommStatus
    COMMS_FLAGS_WIFI_AP_CONFIG_STATUS           = (1<< 7),  // get/set WifiAPConfigStatus
    COMMS_FLAGS_WIFI_WPS_CONFIG_STATUS          = (1<< 8),  // get/set WifiWPSConfigStatus
    COMMS_FLAGS_WIFI_SLEEP_STATUS               = (1<< 9),  // get/set WifiSleepStatus

    // cellular/mobile comms
    COMMS_FLAGS_4G_CONN_STATUS                  = (1<<10),  // get/set 4gConnStatus
    COMMS_FLAGS_4G_TIME_SYNC_STATUS             = (1<<11),  // get/set 4gTimeSyncStatus
    COMMS_FLAGS_NETWORK_OPERATOR_STATUS         = (1<<12),  // get/set NetworkOperatorStatus
    COMMS_FLAGS_SIM_STATUS                      = (1<<13),  // get/set SimStatus
    COMMS_FLAGS_RSSI_FAULT_STATUS               = (1<<14),  // get/set RssiFault

    // LoRa Mesh comms
    COMMS_FLAGS_MESH_CONN_STATUS                = (1<<15),  // get/set MeshConnStatus
    COMMS_FLAGS_LORA_TIME_SYNC_STATUS           = (1<<16),  // get/set LoraTimeSyncStatus
    COMMS_FLAGS_LORA_NODE_DETECTED_STATUS       = (1<<17),  // get/set LoraNodeDetectedStatus

    // local/peripheral comms
    COMMS_FLAGS_MCU_TO_MODEM_COMMS_STATUS       = (1<<18),  // get/set McutoModemCommsStatus
    COMMS_FLAGS_MCU_TO_ENMTR_COMMS_STATUS       = (1<<19),  // get/set MCU2EnmtrStatus
    COMMS_FLAGS_MCU_TO_LORA_COMMS_STATUS        = (1<<20),  // get/set MCU2LoRaStatus
} comms_flags_masks_et; // comms flags bit masks


/*
 * Public Function Prototypes
 */
void systemFlagsInit(void);
uint32_t getSystemFlags(void);
void setSystemFlags(uint32_t u32_mask, bool b_set /*false = bitwise clear*/);
uint32_t getCommsFlags(void);
void setCommsFlags(uint32_t u32_mask, bool b_set /*false = bitwise clear*/);

#define getSystemFlag(flag)             (0 != (getSystemFlags() & SYSTEM_FLAGS_ ## flag ## _STATUS) ? true : false)
#define setSystemFlag(flag, state)      setSystemFlags(SYSTEM_FLAGS_ ## flag ## _STATUS, (state))

#define getCommsFlag(flag)              (0 != (getCommsFlags() & COMMS_FLAGS_ ## flag ## _STATUS) ? true : false)
#define setCommsFlag(flag, state)       setCommsFlags(COMMS_FLAGS_ ## flag ## _STATUS, (state))

#ifdef __cplusplus
}
#endif

#endif // __SYSTEM_FLAGS_H__
