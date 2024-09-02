
#pragma once

#include "modem/quectel.h"
#include "dtls/dtls_client.h"


namespace modem
{


namespace pdp
{

typedef struct
{
    uint8_t aui8_buff[DTLS_MAX_BUF];        // UDP payload = DTLS message
    size_t  sz_length;                      // content length (non zero means it is being used)
} payload_buffer_st;

typedef struct
{
    // scheme e.g. UDP or TCP.  (UDP only, for now)
    CellularModem::pdp_protocol_et  e_protocol;
    // remote/host address (either IPv4 octets or [sub+]domain name)
    char                            ac_address[64];
    // remote port (aka channel)
    unsigned                        u_port;
    // call-back function to be called when incoming data arrives
    bool                            (*fpv_receive_cb)(const uint8_t *pui8_data, uint16_t ui16_length);
} session_config_st;

/*
 * Public Function Prototypes
 */
int request_session(const session_config_st *ps_config);
bool close_session(int id_session);
bool send_data(int id_session, const uint8_t *pui8_data, uint16_t ui16_length);
int get_error(void);


/* Exclusive Functions for Modem-Manager only */
// init & cycle routines
bool init_sessions(void);
bool process_sessions(void);
// call backs functions
void handle_session_status(int id_connect, int n_status);
void handle_incoming_data(int id_connect, unsigned num_bytes);

} // namespace modem::pdp

} // namespace modem
