#ifndef _KERNEL_TEST_DMA_REDMULE_H_
#define _KERNEL_TEST_DMA_REDMULE_H_

void kernel_test_dma_and_redmule()
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
        flex_redmule_set_M(4);
        flex_redmule_set_N(32);
        flex_redmule_set_K(4);
        flex_redmule_trigger_block();
        flex_redmule_trigger_block();
    }

    flex_global_barrier();
}

#endif