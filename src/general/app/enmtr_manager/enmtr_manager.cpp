
#include "global_defs.h"
#include "general_info.h"
#include "enmtr_manager.h"


namespace enmtr::manager
{


/*
 * Local Variables
 */

ENMTR_CLASS mtr;

static state_et             e_state;

/*
 * Public Functions
 */
bool init()
{
    LOGD("core %u: %s", xPortGetCoreID(), __PRETTY_FUNCTION__);

    mtr.init();

    e_state = STATE_POLL_INIT;

    return true;
}

void cycle()
{
    switch (e_state)
    {
    case STATE_POLL_INIT:
        if (false == mtr.reset())
        {
            LOGW("enmtr reset failed");
        }
        else if ((false == mtr.read_versions(mtr.DEVICE_PHASE_A)) ||
                 (false == mtr.read_versions(mtr.DEVICE_PHASE_B)) ||
                 (false == mtr.read_versions(mtr.DEVICE_PHASE_C)))
        {
            //
        }
        else
        {
            LOGI("energy meter A: v%02x.%02x.%04x", mtr.s_fw_info[mtr.DEVICE_PHASE_A].u16_ver >> 8,
                mtr.s_fw_info[mtr.DEVICE_PHASE_A].u16_ver & 0xff, mtr.s_fw_info[mtr.DEVICE_PHASE_A].u16_test);
            LOGI("energy meter B: v%02x.%02x.%04x", mtr.s_fw_info[mtr.DEVICE_PHASE_B].u16_ver >> 8,
                mtr.s_fw_info[mtr.DEVICE_PHASE_B].u16_ver & 0xff, mtr.s_fw_info[mtr.DEVICE_PHASE_B].u16_test);
            LOGI("energy meter C: v%02x.%02x.%04x", mtr.s_fw_info[mtr.DEVICE_PHASE_C].u16_ver >> 8,
                mtr.s_fw_info[mtr.DEVICE_PHASE_C].u16_ver & 0xff, mtr.s_fw_info[mtr.DEVICE_PHASE_C].u16_test);
            sprintf(enmtr_version,"%02x.%02x.%04x", mtr.s_fw_info[mtr.DEVICE_PHASE_A].u16_ver >> 8,
                mtr.s_fw_info[mtr.DEVICE_PHASE_A].u16_ver & 0xff, mtr.s_fw_info[mtr.DEVICE_PHASE_A].u16_test);
            e_state = STATE_POLL_VRMS;
            break;
        }
        delayms(3 * 1000UL);
        break;

    case STATE_POLL_VRMS:
        // to do
        break;

    default:
        e_state = STATE_POLL_INIT;
        break;
    }
}

} // namespace enmtr::manager
