
#include  <sys/lock.h>
#include "system_flags.h"

/*
 * Local Variables
 */
static uint32_t u32_system_flags = 0;
static uint32_t u32_comms_flags = 0;
static _lock_t  lock = (void *)0;


/*
 * Public Functions
 */
void systemFlagsInit(void)
{
    u32_system_flags = 0;
    u32_comms_flags = 0;
}

uint32_t getSystemFlags(void)
{
    uint32_t flags = 0;

    _lock_acquire(&lock);
    flags = u32_system_flags;
    _lock_release(&lock);

    return flags;
}

void setSystemFlags(uint32_t u32_mask, bool b_set /*false = bitwise clear*/)
{
    _lock_acquire(&lock);
    if (b_set) {
        u32_system_flags |= u32_mask;
    } else {
        u32_system_flags &= ~u32_mask;
    }
    _lock_release(&lock);
}

uint32_t getCommsFlags(void)
{
    uint32_t flags = 0;

    _lock_acquire(&lock);
    flags = u32_comms_flags;
    _lock_release(&lock);

    return flags;
}

void setCommsFlags(uint32_t u32_mask, bool b_set /*false = bitwise clear*/)
{
    _lock_acquire(&lock);
    if (b_set) {
        u32_comms_flags |= u32_mask;
    } else {
        u32_comms_flags &= ~u32_mask;
    }
    _lock_release(&lock);
}
