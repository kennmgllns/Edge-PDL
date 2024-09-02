
#include "global_defs.h"
#include "lora_mesh.h"


namespace lora::mesh
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

} // namespace lora::mesh
