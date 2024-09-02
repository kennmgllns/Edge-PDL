
#pragma once

#include "modem/quectel.h"
#include "modem_pdp.h"


namespace modem
{

/*
 * Global Constants
 */

typedef enum
{
    STATE_INIT = 0,         // reset modem
    STATE_AT_CHECK,         // check if responding to a simple AT command
    STATE_AT_CONFIG,        // configure AT modem comms
    STATE_MODEM_INFO,       // read modem info
    STATE_SIM_INFO,         // read SIM info
    STATE_NET_STATUS,       // read network status (network operator, etc.)
    STATE_DATA_CNX,         // manage PDP connection
    STATE_DATA_SESSIONS,    // manage PDP sessions (e.g. UDP transactions)
    STATE_IDLE
} state_et;


typedef enum
{
    CONFIG_STATE_INIT_CONFIG,           // command echo off, flow control, etc.
    CONFIG_STATE_ENABLE_CMEE,           // enable MT error reporting
    CONFIG_STATE_ENABLE_CREG_URC,       // enable network registration unsolicited result code
} config_state_et;

typedef enum
{
    NETWORK_STATE_GET_SIGNAL_QUALITY,   // csq/cesq/qcsq
    NETWORK_STATE_GET_REGISTRATION,     // creg/cereg
    NETWORK_STATE_GET_OPERATOR,         // cops
  //NETWORK_STATE_GET_NETWORK_BAND,     // (included in cell info)
    NETWORK_STATE_GET_CELL_INFO,        // qeng+servingcell
} network_state_et;

typedef enum
{
    PDPCTX_STATE_LOOKUP_APN,            // get APN's based on IMSI
    PDPCTX_STATE_VERIFY_APN,            // check if already set
    PDPCTX_STATE_SET_APN,               // set APN's
    PDPCTX_ACTIVATE,                    // initiate activation
    PDPCTX_ACTIVATING,                  // wait for 150sec (most)
    PDPCTX_ACTIVATED,                   // get IP address of active context
    // ...
    PDPCTX_STATE_READY,                 // ready for sessions/sockets
} pdpctx_state_et;


namespace manager
{

/*
 * Public Function Prototypes
 */
bool init();
void cycle();

/* exclusive for modem-manager task */
void reset(bool b_full_reset);
bool has_data_connection(void);

/* public functions for other tasks */
bool get_connection_state(bool *pb_state /*true=connected*/, const char **p_address, const char **p_apn);


} // namespace modem::manager

} // namespace modem
