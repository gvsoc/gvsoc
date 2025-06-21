#include "flex_runtime.h"
#include "deepseek_config.h"
#include "vector_lib.h"
// #include "deepseek_moe_gate.h"
#include "deepseek_moe_scatter.h"
#include <math.h>

#define SEQ_LEN 8192
#define HBM_INTERVAL_OFFSET (DEEPSEEK_EMBEDDED_LENGTH * SEQ_LEN)

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

    DeepSeek_MoE_scatter(
        hbm_west(0,0) + 2 * HBM_INTERVAL_OFFSET/*input_address*/,
        hbm_west(0,0)/*index_address*/,
        hbm_west(0,0) + HBM_INTERVAL_OFFSET/*locat_address*/,
        hbm_west(0,0) + 3 * HBM_INTERVAL_OFFSET/*scatter_address*/,
        SEQ_LEN/*seq_length*/);

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}