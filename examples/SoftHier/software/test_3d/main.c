#include "flex_runtime_3d.h"
#include <math.h>


void hello_world_all_cluster(){
    flex_global_barrier_xyz();//Global barrier
    for (int cid = 0; cid < ARCH_NUM_CLUSTER; ++cid)
    {
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == cid)
        {
            FlexPosition pos = get_pos(cid);
            printf("[Cluster %3d] Hello from (%d, %d, %d)\n", cid, pos.x, pos.y, pos.z);
        }
        flex_global_barrier_xyz();//Global barrier
    }
    flex_global_barrier_xyz();//Global barrier
}


int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xyz_init();
    flex_global_barrier_xyz();

    //*************************************
    //  Program Execution Region -- Start *
    //*************************************

    hello_world_all_cluster();

    //**************************************
    //*  Program Execution Region -- Stop  *
    //**************************************
    flex_global_barrier_xyz();
    flex_eoc(eoc_val);

    return 0;
}
