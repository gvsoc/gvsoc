#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"
#include "kernels/common/common_tiling.h"
#include "kernels/llm_mlp/llm_mlp_matmul.h"
#include "kernels/llm_mlp/llm_mlp_inter_cluster_matmul.h"
#include "kernels/kernel_test_dma_redmule.h"
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
    // if (flex_is_first_core())
    // {
    //     flex_redmule_set_M(0, 256);
    //     flex_redmule_set_N(0, 256);
    //     flex_redmule_set_K(0, 256);
    //     flex_redmule_trigger_async(0);
    //     flex_redmule_trigger_wait(0);
    // }
    // multi_redmule_prepare(8, 128, 2, 16);
    // multi_redmule_trigger();
    // llm_mlp_inter_cluster_matmul_optimize_hbm_west_south_placement(256,2,ARCH_NUM_CLUSTER_X);
    flex_barrier_neighbor_init();
    for (int i = 0; i < 10; ++i)
    {
        flex_neighbor_barrier();
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    // flex_timer_end();
    flex_eoc(eoc_val);
	return 0;
}