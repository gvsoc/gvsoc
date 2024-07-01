#include "snrt.h"
#include "eoc.h"

int main()
{
    float * local_x  = 0x20000 * snrt_cluster_idx();
    float * remote_x = 0xc0000000 + 0x20000 * snrt_cluster_idx();

    snrt_cluster_hw_barrier(); //Global Snitch Cluster Barrier

    if (snrt_is_dm_core()) {
        size_t size = 4096; //4KiB
        snrt_dma_start_1d(local_x, remote_x, size); //Start iDMA
        snrt_dma_wait_all(); // Wait for iDMA Finishing
    }

    snrt_cluster_hw_barrier();
    eoc(1);
	return 0;
}