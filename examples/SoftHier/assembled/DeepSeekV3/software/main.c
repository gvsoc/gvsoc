#include "flex_runtime.h"
#include "deepseek_config.h"
#include "vector_lib.h"
// #include "deepseek_moe_gate.h"
// #include "deepseek_moe_scatter.h"
#include "deepseek_summa_2d.h"
#include <math.h>

#define SEQ_LEN 8192
#define HBM_INTERVAL_OFFSET (DEEPSEEK_EMBEDDED_LENGTH * SEQ_LEN)

#define SUMMA_M_TILE 128
#define SUMMA_GROUP_X 16
#define SUMMA_GROUP_Y 32
#define SUMMA_GROUPS  2

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    // DeepSeek_MoE_gate_C32x32(
    //     hbm_south(0,0)/*input_address*/,
    //     hbm_south(0,0)/*weight_address*/,
    //     hbm_south(0,0)/*index_address*/,
    //     hbm_south(0,0)/*score_address*/,
    //     8192/*seq_length*/);

    // DeepSeek_MoE_scatter(
    //     hbm_west(0,0) + 2 * HBM_INTERVAL_OFFSET/*input_address*/,
    //     hbm_west(0,0)/*index_address*/,
    //     hbm_west(0,0) + HBM_INTERVAL_OFFSET/*locat_address*/,
    //     hbm_west(0,0) + 3 * HBM_INTERVAL_OFFSET/*scatter_address*/,
    //     SEQ_LEN/*seq_length*/);

    DeepSeek_SUMMA_2D(
        hbm_west(0,0)/*X_address*/,
        hbm_south(0,0)/*W_address*/,
        hbm_west(0,0)/*Z_address*/,
        SEQ_LEN/*M_size*/,
        2048/*N_size*/,
        7168/*K_size*/, /*shared dimension*/
        SUMMA_M_TILE/*M_tile*/,
        128/*N_tile*/,
        128/*K_tile*/,
        SUMMA_GROUP_X/*group_x*/,
        SUMMA_GROUP_Y/*group_y*/,
        SUMMA_GROUPS/*num_group*/,
        (DEEPSEEK_EMBEDDED_LENGTH * SEQ_LEN * DATA_TYPE_BYTE)/*X_address_group_gap*/,
        (DEEPSEEK_EMBEDDED_LENGTH * 2048    * DATA_TYPE_BYTE)/*W_address_group_gap*/,
        (DEEPSEEK_EMBEDDED_LENGTH * SEQ_LEN * DATA_TYPE_BYTE)/*Z_address_group_gap*/);

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}