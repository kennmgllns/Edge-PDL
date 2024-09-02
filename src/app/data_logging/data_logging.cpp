
#include "global_defs.h"
#include "data_logging.h"
namespace data::logging
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
     LOGD("dl cyc: ");

}

} // namespace data::logging
