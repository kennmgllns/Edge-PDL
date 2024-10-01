/*
 * mesh_cloud.h
 *
 */

#ifndef _MESH_CLOUD_H_
#define _MESH_CLOUD_H_

#include "mesh_commands.h"
#include "mesh_update.h"

/*
 * Global Structs
 */
typedef struct
{
#if 0
    struct {
        // phy info
    } s_lora;
    struct {
        // status info
    } s_stats;
#endif
    struct {
        uint32_t    ui32_parent_id; // 4G-connected node
        uint32_t    ui32_hop_id;    // first node hop, if any
        int16_t     i16_rssi;       // parent node rssi
    } s_node; // child node info
} mesh_cloud_info_st;


/*
 * Public Function Prototypes
 */

/* for non-4G connected node (aka "child" node) */
void meshCloudInit(bool b_full /*false = reset states only*/);
bool meshCloudCycle(void);
bool meshCloudParseReportAck(const mesh_comms_payload_st *ps_payload);
void meshCloudParseServerResponse(const mesh_comms_payload_st *ps_payload);
uint32_t meshCloudGetParentId(void);
bool meshCloudGetInfo(mesh_cloud_info_st *ps_info);

/**
 * >0 - success
 * =0 - in progress
 * <0 = failed 
 */
int meshCloudSync(bool b_forced);
uint32_t meshCloudGetLastSync(void);
bool meshCloudParseSyncResponse(const mesh_comms_payload_st *ps_payload);

#endif /* _MESH_CLOUD_H_ */
