
#pragma once

#include "cloud_net.h"
#include "cloud_comms_cfg.h"


namespace cloud
{

/*
 * Global Constants
 */
typedef enum
{
    STATE_INIT = 0,           // initialize
    STATE_DTLS_SESSION,       // manage DTLS over UDP connection
    STATE_SYNC_TIME,          // synchronize with server time
    STATE_SEND_REPORTS,       // send monitor, status & events reports
    STATE_SERVER_REQUESTS,    // process fw-update requests, operation commands, etc.
    STATE_IDLE
} state_et;

typedef enum
{
    UDP_STATE_INIT,
    UDP_STATE_CONNECT,        // request UDP session from modem manager
    UDP_STATE_CONNECTING,
    UDP_STATE_CLOSE,
    UDP_STATE_READY,
    UDP_STATE_IDLE
} udp_state_et;

typedef enum
{
    DTLS_STATE_INIT,
    DTLS_STATE_CONNECT,
    DTLS_STATE_CONNECTING,
    DTLS_STATE_CLOSE,
    DTLS_STATE_READY,
    DTLS_STATE_IDLE
} dtls_state_et;

namespace comms
{

/*
 * Public Function Prototypes
 */
bool init();
void cycle();

void connect(void);     // [re]connect
void disconnect(void);  // close dtls & udp session
void flush_buff(void);  // cancel pending udp send queue


} // namespace cloud::comms

} // namespace cloud
