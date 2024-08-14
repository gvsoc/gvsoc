#ifndef _KERNEL_TEST_DMA_REDMULE_H_
#define _KERNEL_TEST_DMA_REDMULE_H_
#include <math.h>
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

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
        flex_redmule_set_M(0, 4);
        flex_redmule_set_N(0, 32);
        flex_redmule_set_K(0, 4);
        flex_redmule_trigger_block(0);
        flex_redmule_trigger_block(0);
    }

    flex_global_barrier();
}

void multi_redmule_prepare(uint32_t tile_dimension, uint32_t long_dimension, uint32_t elem_size, uint32_t redmule_per_row){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    if (flex_is_first_core())
    {
        for (int i = 0; i < ARCH_NUM_REDMULE_PER_CLUSTER; ++i)
        {
            flex_redmule_set_M(i, tile_dimension);
            flex_redmule_set_N(i, long_dimension);
            flex_redmule_set_K(i, tile_dimension);

            //location
            uint32_t row = i/redmule_per_row;
            uint32_t col = i%redmule_per_row;
            flex_redmule_set_X(i, row * tile_size_byte * redmule_per_row);
            flex_redmule_set_W(i, col * tile_size_byte);
            flex_redmule_set_Y(i, row * tile_size_byte * redmule_per_row + col * tile_size_byte);
            flex_redmule_set_Z(i, row * tile_size_byte * redmule_per_row + col * tile_size_byte);
        }
    }
}

void multi_redmule_trigger(){
    if (flex_is_first_core())
    {
        for (int i = 0; i < ARCH_NUM_REDMULE_PER_CLUSTER; ++i)
        {
            flex_redmule_trigger_async(i);
        }

        for (int i = 0; i < ARCH_NUM_REDMULE_PER_CLUSTER; ++i)
        {
            flex_redmule_trigger_wait(i);
        }
    }
}

#endif