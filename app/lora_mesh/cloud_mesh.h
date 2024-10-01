/*
 * cloud_mesh.h
 * "cloud_mesh" - for parent nodes (e.g. 4G-connected)
 * "mesh_cloud" - for child nodes
 */

#ifndef _CLOUD_MESH_H_
#define _CLOUD_MESH_H_

#include "mesh_comms.h"


/*
 * Public Function Prototypes
 */


/* for 4G connected node (aka "parent" node) */
void cloudMeshInit(void);
void cloudMeshCycle(void);
void cloudMeshParseResponse(uint16_t u16_msg_id, const uint8_t *pu8_buf, size_t sz_len);

bool meshCloudCurrentTime(uint32_t u32_dst_id, const mesh_comms_payload_st *ps_payload);
bool meshCloudHandleRequest(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload);
bool meshCloudParseResponseAck(uint32_t u32_id_from, const mesh_comms_payload_st *ps_payload);

#endif /* _CLOUD_MESH_H_ */
