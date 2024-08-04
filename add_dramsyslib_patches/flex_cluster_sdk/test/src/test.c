#include "snrt.h"
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"
#include "kernels/common/common_tiling.h"
#include "kernels/llm_mlp/llm_mlp_matmul.h"
#include "kernels/llm_mlp/llm_mlp_inter_cluster_matmul.h"
#include <math.h>

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    // flex_timer_start();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    // llm_mlp_matmul(10, 64, 2);
    // llm_mlp_test(2048);
    // llm_mlp_inter_cluster_test_hbm(128,2);
    // llm_mlp_inter_cluster_matmul(1024,2,ARCH_NUM_CLUSTER_X);
    // llm_mlp_inter_cluster_matmul_optimize_hbm(32,2,ARCH_NUM_CLUSTER_X);
    // llm_mlp_test(1024,2,ARCH_NUM_CLUSTER_X);
    if (flex_is_first_core())
    {
        flex_redmule_set_M(32);
        flex_redmule_set_N(128);
        flex_redmule_set_K(32);
        flex_redmule_trigger_block();
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    // flex_timer_end();
    flex_eoc(eoc_val);
	return 0;
}