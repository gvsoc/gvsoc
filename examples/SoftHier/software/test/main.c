#include "flex_runtime.h"
#include "hello_world.h"
#include <math.h>

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    test_nm_config();

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}