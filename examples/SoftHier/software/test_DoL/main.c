#include "flex_runtime.h"
#include "test_dol_channel.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();

    test_dol_channel_all();

    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}
