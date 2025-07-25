#include "flex_runtime.h"
// #include "hello_world.h"
#include "MLA_decode_MHA.h"
#include <math.h>

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    MLA_Decode_MHA(
        hbm_west(0,0)/*MetaQ_base_address*/,
        hbm_south(0,0)/*CKVR_base_address*/,
        hbm_west(0,0)/*Output_base_address*/,
        4,
        512,
        8,
        DATA_TYPE_BYTE/*elem_size*/);

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}