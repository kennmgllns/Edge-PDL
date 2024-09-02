
#include "global_defs.h"
#include "heartbeat.h"
namespace heartbeat
{

/*
 * Public Functions
 */
bool init()
{
   // LOGD("core %u: %s", xPortGetCoreID(), __PRETTY_FUNCTION__);
    LOGD("task_hb_init\n");
    return true;
}

void cycle()
{
    LOGD("hb cyc: ");
   // LOGD("%s %u", __PRETTY_FUNCTION__, millis());
}

} // namespace heartbeat
