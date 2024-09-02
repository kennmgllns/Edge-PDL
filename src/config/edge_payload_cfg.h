
#pragma once

/*
 * Configurable Constants
 */
#define K_PAYLOAD_MAX_DEV_ID_LEN            (8)                                             // 8 bytes max device id length
#define K_PAYLOAD_MAX_MON_PARAM_COUNT       (52)                                            // 30 monitor parameters max per phase
#define K_PAYLOAD_MAX_MON_PARAM32_COUNT     (6)                                             // 6 32-bit monitor parameters max per phase
#define K_PAYLOAD_MAX_MON_PHASE_COUNT       (4)                                             // 4 monitor phase max per payload
#define K_PAYLOAD_MAX_STATUS_TAG_COUNT      (3)                                             // 3 status tag max per payload
#define K_PAYLOAD_MAX_EVENTS_COUNT          (3)                                             // 3 events max per payload

#define K_PAYLOAD_PROTOCOL_VER              (EP_PROTOCOL_VER_LINEAR11)
#define K_PAYLOAD_DEVICE_TYPE               (EP_DEVICE_TYPE_PDL)
