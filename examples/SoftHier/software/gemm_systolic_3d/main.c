#include "flex_runtime_3d.h"
#include "gemm_systolic_wise.h"
#include <math.h>

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xyz_init();
    flex_global_barrier_xyz();

    //*************************************
    //  Program Execution Region -- Start *
    //*************************************

    gemm_systolic_wise(4096, 4096, 4096, 2, 256, 256, 256);

    //**************************************
    //*  Program Execution Region -- Stop  *
    //**************************************
    flex_global_barrier_xyz();
    flex_eoc(eoc_val);

    return 0;
}
