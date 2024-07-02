#include "snrt.h"
#include "eoc.h"
#include "flex_cluster_arch.h"

int main()
{
    // float * local_x  = ARCH_CLUSTER_TCDM_SIZE * snrt_cluster_idx();
    // float * remote_x = ARCH_HBM_START_BASE + ARCH_CLUSTER_TCDM_SIZE * snrt_cluster_idx();

    // snrt_cluster_hw_barrier(); //Global Snitch Cluster Barrier

    // if (snrt_is_dm_core()) {
    //     size_t size = 4096; //4KiB
    //     snrt_dma_start_1d(local_x, remote_x, size); //Start iDMA
    //     snrt_dma_wait_all(); // Wait for iDMA Finishing
    // }
    flex_barrier_init();
    flex_global_barrier(ARCH_NUM_CLUSTER_X*ARCH_NUM_CLUSTER_Y);
    eoc(get_cluster_id());
	return 0;
}