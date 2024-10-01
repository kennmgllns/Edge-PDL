
#include "global_defs.h"
#include "input_monitoring.h"


namespace input::monitoring
{

/*
 * Public Functions
 */
bool init()
{
    LOGD("task_hb_init\n");
   // LOGD("core %u: %s", xPortGetCoreID(), __PRETTY_FUNCTION__);
    return true;
}

void cycle()
{
    //
}

} // namespace input::monitoring
