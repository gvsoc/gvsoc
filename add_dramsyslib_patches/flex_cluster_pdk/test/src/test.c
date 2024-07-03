#include "snrt.h"
#include "flex_runtime.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

int main()
{
    
    flex_barrier_init();
    flex_global_barrier();

    if (snrt_is_dm_core()){
        size_t size = ARCH_CLUSTER_TCDM_SIZE/2;
        flex_dma_pattern_all_to_one(0, size, size);
    }

    flex_global_barrier();
    snrt_cluster_hw_barrier();
    flex_eoc(1);
	return 0;
}