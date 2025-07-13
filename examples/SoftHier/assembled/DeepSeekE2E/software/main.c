#include "flex_runtime.h"
#include "dsv3_hbm_map.h"
#include <math.h>


int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
    {
        DSv3HBMMapInfo info = DSv3HBMMapInfoAnalyze(
            128/*Batch Size*/,
            4/*Speculative Length*/,
            4096/*KV Cache Length*/);
        printDSv3HBMMapInfo(&info);
    }


    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}