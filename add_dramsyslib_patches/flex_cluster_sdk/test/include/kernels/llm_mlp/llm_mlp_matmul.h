#ifndef _LLM_MLP_MATMUL_H_
#define _LLM_MLP_MATMUL_H_
#include <math.h>
#include "kernels/common/common_tiling.h"
#include "snrt.h"
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

void llm_mlp_matmul_routine(uint32_t M, uint32_t N, uint32_t K, uint32_t tile_size_byte)
{
    if (flex_is_dm_core()){
        flex_dma_pattern_access_west_hbm(tile_size_byte, 0, tile_size_byte);
        flex_dma_pattern_access_west_hbm(tile_size_byte*2, 0, tile_size_byte);
    }

	if (flex_is_first_core())
    {
        flex_redmule_set_M(M);
        flex_redmule_set_N(N);
        flex_redmule_set_K(K);
        flex_redmule_trigger_block();
    }
}

void llm_mlp_matmul_preload(uint32_t tile_size_byte)
{
    if (flex_is_dm_core()){
        flex_dma_pattern_access_west_hbm(tile_size_byte, 0, tile_size_byte);
        flex_dma_pattern_access_west_hbm(tile_size_byte*2, 0, tile_size_byte);
    }
}

void llm_mlp_matmul_final_compute(uint32_t M, uint32_t N, uint32_t K)
{
    if (flex_is_first_core())
    {
        flex_redmule_set_M(M);
        flex_redmule_set_N(N);
        flex_redmule_set_K(K);
        flex_redmule_trigger_block();
    }
}

void llm_mlp_matmul_store(uint32_t tile_size_byte)
{
    if (flex_is_dm_core()){
        flex_dma_pattern_access_west_hbm(tile_size_byte, 0, tile_size_byte);
    }
}

void llm_mlp_matmul(uint32_t num_iter, uint32_t tile_dimension, uint32_t elem_size)
{
	uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

	//Preloading
	llm_mlp_matmul_preload(tile_size_byte);
	snrt_cluster_hw_barrier();

	//Routine: dma + redmule
	for (int i = 0; i < (num_iter-1); ++i)
	{
		llm_mlp_matmul_routine(tile_dimension,tile_dimension,tile_dimension,tile_size_byte);
		snrt_cluster_hw_barrier();
	}

	//Final compute
	llm_mlp_matmul_final_compute(tile_dimension,tile_dimension,tile_dimension);
	snrt_cluster_hw_barrier();

	//Store output matrix
	llm_mlp_matmul_store(tile_size_byte);
	snrt_cluster_hw_barrier();
}

void llm_mlp_test(uint32_t tile_dimension)
{
    uint32_t num_iter = 4096/tile_dimension;
    uint32_t num_matmul = (8192/(tile_dimension*ARCH_NUM_CLUSTER_Y)) * (16384/(tile_dimension*ARCH_NUM_CLUSTER_X));

    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_matmul; ++i)
    {
        llm_mlp_matmul(num_iter, tile_dimension, 2);
    }

    flex_global_barrier_xy();
    flex_timer_end();
}

#endif