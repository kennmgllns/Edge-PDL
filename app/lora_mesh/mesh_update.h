/*
 * mesh_update.h
 * cloud fw-update via mesh
 */

#ifndef _MESH_UPDATE_H_
#define _MESH_UPDATE_H_


/*
 * Public Function Prototypes
 */
void meshUpdateInit(void);
bool meshUpdateCycle(void);

bool meshUpdateParseRequestAck(const mesh_comms_payload_st *ps_payload);
void meshUpdateParseServerResponse(const mesh_comms_payload_st *ps_payload);

#endif /* _MESH_UPDATE_H_ */
