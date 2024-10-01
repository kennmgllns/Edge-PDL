/*
 * mesh_route.h
 *
 */

#ifndef _MESH_ROUTE_H_
#define _MESH_ROUTE_H_

#include <stdbool.h>
#include <stdint.h>


#define NUM_NODES_PER_MSG       (48)    // 12 + (N * 5) + 2 < 255;
#define NODE_INFO_SIZE          (5)     // 4-byte ID & 1-byte status (flag and hop count)
#define MESH_MAX_HOP_COUNT      (30)    // 5-bit (0x1F is reserved)


/** node status flags */
typedef enum node_status {
    NODE_CLOUD_NET_AVAILBLE     = 1<<0, // connected directly to cloud, e.g. via 4G
    //                          = 1<<1, // TBD (reserved)
    //                          = 1<<2, // TBD (reserved)
} node_status_et;

typedef struct mesh_node {
    /* 5-byte node info */
    uint32_t u32_id;            // node id
    uint8_t  u3_flags:3;        // status flags (see "node_status_et")
    uint8_t  u5_num_hops:5;     // number of hops (up to 30 hops only, "0x1F" is reserved)
    /* internal use */
    int8_t   i8_snr;            // signal to noise ratio (dB) - for direct-connected node only
    int16_t  i16_rssi;          // received signal strength indication (dBm) - for direct-connected node only
    uint32_t u32_first_hop;     // node used for forwarding (i.e. not direct connected)
    uint32_t ms_timestamp;      // timestamp of when info was updated
} mesh_node_st;

#define MESH_NODE_FLAGS(u8)         (((u8) & 0xE0) >> 5)
#define MESH_NODE_HOPS(u8)          ((u8) & 0x1F)
#define MESH_NODE_STATUS(f, h)      ((((f) & 7) << 5) | ((h) & 0x1f))


void meshRouteInit(void);
bool getRoute(uint32_t id, mesh_node_st *route);
bool getCloudNode(mesh_node_st *cloudnode, uint32_t id_ignore);
bool addNode(uint32_t u32_id, uint32_t u32_hop,
             uint8_t u5_num_hops, uint8_t u3_flags,
             int8_t i8_snr, int16_t i16_rssi);
void clearSubs(uint32_t id);
bool cleanMap(void);
uint16_t nodeMap(uint8_t nodes[][NODE_INFO_SIZE], uint16_t u16_offset);
uint16_t numOfNodes();
bool getNode(uint8_t nodeNum, uint32_t *nodeId, uint32_t *firstHop, uint8_t *numHops);
void printNodes(void);

uint32_t getNextBroadcastID(void);
bool isOldBroadcast(uint32_t broadcastID);

#endif /* _MESH_ROUTE_H_ */
