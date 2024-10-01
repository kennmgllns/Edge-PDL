/*
 * mesh_commands.h
 * cloud commands via mesh
 */

#ifndef _MESH_COMMANDS_H_
#define _MESH_COMMANDS_H_


/*
 * Public Function Prototypes
 */

void meshCommandInit(void);
bool meshCommandCycle(void);

bool meshCommandParseRequestAck(const mesh_comms_payload_st *ps_payload);
void meshCommandParseServerResponse(const mesh_comms_payload_st *ps_payload);

#endif /* _MESH_COMMANDS_H_ */
