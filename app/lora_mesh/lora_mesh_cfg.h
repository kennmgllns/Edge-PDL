/*
 * lora_mesh_cfg.h
 *
 */

#ifndef _LORA_MESH_CFG_H_
#define _LORA_MESH_CFG_H_


#define MESH_MSG_HEADER_MARK    "LRM", 3   /* LoRa-Mesh */

#define MESH_MAX_NUM_MAP_MSG    (5)  /* up to 240 (= 48 x 5) nodes per mesh */
#define MESH_MAX_NUM_NODES      (NUM_NODES_PER_MSG * MESH_MAX_NUM_MAP_MSG)

/** Max number of messages in the queue */
#define MESH_SEND_QUEUE_SIZE    (8)

/** comment out below to connect to any parent */
//#define MESH_PARENT_NODE_ID     (0x609ca19a)

/** show diagnostics counters on log prints */
// #define MESH_ENABLE_PRINT_DIAGNOSTICS

/** milliseconds timeout after 4G comms lost */
#define MESH_CLOUD_OFFLINE_TIMEOUT  (2 * 60 * 1000UL) // (10 * 60 * 1000UL)


/** Sync time for routing at start */
#define MESH_INIT_SYNCTIME      30000
/** Sync time for routing after mesh has settled */
#define MESH_DEFAULT_SYNCTIME   60000
/** Time to switch from INIT_SYNCTIME to DEFAULT_SYNCTIME */
#define MESH_SWITCH_SYNCTIME    300000

/** Timeout to remove unresponsive nodes */
#define MESH_NODE_INACTIVE_TIMEOUT   (5 * 60 * 1000UL)


#define LORA_RF_FREQUENCY                   915000000   // Hz
//#define LORA_RF_FREQUENCY                   903900000   // US902_928 Channel 0
//#define LORA_RF_FREQUENCY                   867500000   // EU863_870 Channel 5

#define LORA_TX_OUTPUT_POWER                22          // 22 dBm

#if 1 // default
#define LORA_BANDWIDTH                      LORA_BW_125 // 125kHz
#define LORA_SPREADING_FACTOR               LORA_SF7    // [SF5..SF12]
#else // lower bandwidth, same time-on-air duration
#define LORA_BANDWIDTH                      LORA_BW_062 // 62.5kHz
#define LORA_SPREADING_FACTOR               LORA_SF6    // SF6
#endif
#define LORA_CODINGRATE                     LORA_CR_4_5 // 4/5
#define LORA_PREAMBLE_LENGTH                8           // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                 0           // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON          false
#define LORA_IQ_INVERSION_ON                false


/**
 * Sleep and Listen time definitions
 * Calculated with Semtech SX1261 Calculater
 * SF 7, BW 250, CR 4/5 PreambleLen 8 PayloadLen 253 Hdr enabled CRC enabled
 * 2 -> number of preambles required to detect package
 * 512 -> length of a symbol im ms
 * 1000 -> wake time is in us
 * 15.625 -> SX126x counts in increments of 15.625 us
 *
 * 10 -> max length we can sleep in symbols
 * 512 -> length of a symbol im ms
 * 1000 -> sleep time is in us
 * 15.625 -> SX126x counts in increments of 15.625 us
 */
#define LORA_RX_SLEEP_TIMES                 (2 * 512 * 1000 * 15.625), (10 * 512 * 1000 * 15.625)

/** Number of retries if CAD shows busy */
#define MESH_CAD_RETRY                      (20)


#endif /* _LORA_MESH_CFG_H_ */
