/*
 * mesh_comms.h
 *
 */

#ifndef _MESH_COMMS_H_
#define _MESH_COMMS_H_


/** Max payload size: 255 - 12 [header] - 4 [orig id] - 2 [crc16] */
#define MESH_DATA_MAX_SIZE        (237)


/** mesh comms payload types */
typedef enum mesh_comms_method {
    MESH_COMMS_LOCAL        = 0,    // local messages within the mesh
    MESH_COMMS_REQUEST_GET  = 1,    // client get sent to the cloud server
    MESH_COMMS_REQUEST_PUT  = 2,    // client put or post sent to the cloud server
    MESH_COMMS_RESPONSE     = 3,    // server reply
} mesh_comms_method_et;

/** mesh comms payload endpoint */
typedef enum mesh_comms_endpoint {
    MESH_COMMS_ENDPOINT_GENERAL     = 0,    // for local messages, e.g. ACK
    MESH_COMMS_ENDPOINT_TIME        = 1,    // get "/time"
    MESH_COMMS_ENDPOINT_MONITOR     = 2,    // put "/mn"
    MESH_COMMS_ENDPOINT_STATUS      = 3,    // put|get "/st"
    MESH_COMMS_ENDPOINT_EVENT       = 4,    // put "/st"
    MESH_COMMS_ENDPOINT_HEARTBEAT   = 5,    // put "/hb"
    MESH_COMMS_ENDPOINT_UPDATE      = 6,    // get "/ud"
  //                                = 7,    // (reserved)
} mesh_comms_endpoint_et;

/** payload data header for mesh_data_msg_st's au8_data */
typedef struct mesh_comms_payload_header {
    uint16_t u16_msg_id;    // incremental message id (used as token for replies)
    uint8_t  u8_length;     // payload length
    uint8_t  u2_method:2;   // see "mesh_comms_method_et"
    uint8_t  u3_endpoint:3; // see "mesh_comms_endpoint_et"
    uint8_t  u2_idx:2;      // 0-3 index
    uint8_t  b_partial:1;   // 0 = complete payload
} mesh_comms_payload_header_st; // 4-byte header

/** up to 233 bytes only */
#define MESH_COMMS_PAYLOAD_MAX_SIZE     (MESH_DATA_MAX_SIZE - sizeof(mesh_comms_payload_header_st))

/** comms payload */
typedef struct mesh_comms_payload {
    mesh_comms_payload_header_st  s_hdr;
    uint8_t                       au8_data[MESH_COMMS_PAYLOAD_MAX_SIZE];
} mesh_comms_payload_st;


/**
 * Mesh callback functions
 */
typedef struct
{
    /**
     * @param payload Received buffer pointer
     * @param size    Received buffer size
     * @param rssi    RSSI value computed while receiving the frame [dBm]
     * @param snr     SNR value computed while receiving the frame [dB]
     */
    void (*DataAvailable)(uint32_t fromID, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

    /**
     * Nodes list change callback prototype.
     */
    void (*NodesListChanged)(void);

} mesh_events_st;

extern mesh_events_st  s_mesh_events;

/*
 * Public Function Prototypes
 */
void meshCommsInit(void);
void meshCommsCycle(void);

/* returns message_id (non-zero for success) */
uint16_t meshCommsSend(uint32_t u32_dest_id, uint16_t u16_msg_id /*set to 0 to generate new id*/,
                        const uint8_t *pu8_data, uint8_t u8_length,
                        uint8_t u2_method, uint8_t u3_endpoint,
                        uint8_t u2_idx, uint8_t b_partial);


#endif /* _MESH_COMMS_H_ */
