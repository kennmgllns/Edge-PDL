
#pragma once

namespace lora::mesh
{
    #include "mesh_route.h"
#include "mesh_comms.h"
#include "mesh_cloud.h"


typedef enum {
    MESH_IDLE = 0, //!< The radio is idle
    MESH_RX,       //!< The radio is in reception state
    MESH_TX,       //!< The radio is in transmission state
    MESH_NOTIF     //!< The radio is doing mesh notification
} meshRadioState_t;


/** LoRa package types */
typedef enum mesh_msg_type {
    MESH_MSG_INVALID    = 0,
    MESH_MSG_DIRECT     = 1,
    MESH_MSG_FORWARD    = 2,
    MESH_MSG_BROADCAST  = 3,
    MESH_MSG_NODEMAP    = 4
} mesh_msg_type_et;

typedef struct mesh_msg_header {
    uint8_t  mark[3];   // start token
    uint8_t  u8_type;   // mesh_msg_type_et
    uint32_t u32_src;   // source id
    uint32_t u32_dst;   // destination id
} mesh_msg_header_st;

typedef struct mesh_map_msg {
    mesh_msg_header_st  s_hdr;          // header
    uint8_t             au8_nodes[NUM_NODES_PER_MSG][NODE_INFO_SIZE];
} mesh_map_msg_st;

typedef struct mesh_data_msg {
    mesh_msg_header_st  s_hdr;          // header
    uint32_t            u32_orig;       // orig source id
    uint8_t             au8_data[MESH_DATA_MAX_SIZE + sizeof(uint16_t) /*crc16*/];  // payload buffer
    /* mesh-level internal use */
    uint8_t             _[1];           // padding only
    uint16_t            u16_len;        // message (header & payload) length excluding crc
    uint16_t            u16_crc;        // CRC-16
} mesh_data_msg_st;

typedef struct mesh_diagnostics {
    // LoRa phy layer error counters
    uint32_t            u32_rx_error;       // receive timed out or crc error
    uint32_t            u32_tx_error;       // transmit failed or timed out
    uint32_t            u32_cad_busy;       // RF channel busy
    // mesh layer error counters
    uint32_t            u32_msg_invalid;    // invalid packet
    uint32_t            u32_msg_crc;        // checksum error
    uint32_t            u32_route_fail;     // routing|mapping error
    // app layer error counters
    uint32_t            u32_sync_error;     // unable to sync to parent
    uint32_t            u32_ack_error;      // parent ACK error or timeout
    uint32_t            u32_rsp_error;      // cloud response error or timeout
} mesh_diagnostics_st;

extern mesh_diagnostics_st s_mesh_diag;

uint32_t meshNodeID(void);
#define meshBroadcastID(nid)     ((nid) & 0xFFFFFF00)

bool meshSendRequest(mesh_data_msg_st *package, uint16_t msgSize);

/*
 * Public Function Prototypes
 */
bool init();
void cycle();


} // namespace lora::mesh
