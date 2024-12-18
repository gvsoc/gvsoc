#include "flex_runtime.h"
#include "hello_world.h"
#include <math.h>

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    flex_timer_start();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    test_dma_2d();

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_timer_end();
    flex_eoc(eoc_val);
    return 0;
}