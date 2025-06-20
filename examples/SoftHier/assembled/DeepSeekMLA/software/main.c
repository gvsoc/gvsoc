#include "flex_runtime.h"
// #include "hello_world.h"
// #include "MLA_decode_MHA.h"
#include "SUMMA_2D.h"
#include <math.h>

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    // MLA_Decode_MHA(
    //     hbm_west(0,0)/*MetaQ_base_address*/,
    //     hbm_south(0,0)/*CKVR_base_address*/,
    //     hbm_west(0,0)/*Output_base_address*/,
    //     8/*speculative_length*/,
    //     1024/*kv_sequence_length*/,
    //     8/*batch_size*/,
    //     2/*elem_size*/);

    SUMMA_2D(
        hbm_west(0,0)/*X_address*/,
        hbm_south(0,0)/*W_address*/,
        hbm_west(0,0)/*Z_address*/,
        8192/*M_size*/,
        4096/*N_size*/,
        7168/*K_size*/, /*shared dimension*/
        128/*M_tile*/,
        128/*N_tile*/,
        128/*K_tile*/);

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}