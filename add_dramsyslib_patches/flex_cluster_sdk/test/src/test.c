#include "snrt.h"
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

int main()
{
    flex_barrier_init();
    flex_global_barrier();

    if (flex_is_dm_core()){
        size_t size = ARCH_CLUSTER_TCDM_SIZE/2;
        // flex_dma_pattern_round_shift_left(0, size, size);
        flex_dma_pattern_access_west_hbm(0, 0, size);
    }

    if (flex_is_first_core())
    {
        flex_redmule_set_M(32);
        flex_redmule_set_N(32);
        flex_redmule_set_K(32);
        flex_redmule_trigger_block();
        flex_redmule_trigger_block();
    }

    flex_global_barrier();
    snrt_cluster_hw_barrier();
    flex_eoc(1);
	return 0;
}