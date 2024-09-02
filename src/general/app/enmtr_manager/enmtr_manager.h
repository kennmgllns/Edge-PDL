
#pragma once

#include "enmtr/msp430.h"
#include "enmtr_manager_cfg.h"


namespace enmtr
{

typedef enum
{
    STATE_POLL_INIT,
    STATE_POLL_VRMS,
    STATE_POLL_IRMS,
    STATE_POLL_PF,
    //STATE_POLL_WATT,
    //STATE_POLL_VA,
    STATE_POLL_LOW_RATE,
    STATE_FW_UPDATE
} state_et;


namespace manager
{
/*
 * Public Function Prototypes
 */
bool init();
void cycle();

} // namespace enmtr::manager
} // namespace enmtr
