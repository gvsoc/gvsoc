#ifndef _LLM_MLP_INTER_CLUSTER_MATMUL_H_
#define _LLM_MLP_INTER_CLUSTER_MATMUL_H_
#include <math.h>
#include "kernels/common/common_tiling.h"
#include "snrt.h"
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

void llm_mlp_inter_cluster_test_dual_dma(uint32_t tile_size_byte){
    if (flex_is_dm2_core()){
        flex_dma_pattern_access_west_hbm(tile_size_byte, 0, tile_size_byte);
    }

    if (flex_is_dm_core()){
        flex_dma_pattern_round_shift_left(0, 0, tile_size_byte);
    }
}

void llm_mlp_inter_cluster_test_hbm(uint32_t tile_dimension, uint32_t elem_size){
    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    flex_timer_start();

    if (flex_is_dm2_core()){
        flex_dma_pattern_access_west_hbm(tile_size_byte, 0, tile_size_byte);
        flex_dma_pattern_access_west_hbm(tile_size_byte, 0, tile_size_byte);
    }

    flex_global_barrier_xy();
    flex_timer_end();
}


void llm_mlp_inter_cluster_matmul(uint32_t tile_dimension, uint32_t elem_size, uint32_t num_round_shift){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    //Initialize RedMule Paramters
    if (flex_is_first_core())
    {
        flex_redmule_set_M(0, tile_dimension);
        flex_redmule_set_N(0, tile_dimension);
        flex_redmule_set_K(0, tile_dimension);
    }

    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_round_shift-1; ++i)
    {
        if (flex_is_dm_core()){
            flex_dma_pattern_round_shift_left(0, 0, tile_size_byte);
            flex_dma_pattern_round_shift_up(0, 0, tile_size_byte);
        }

        if (flex_is_first_core())
        {
            flex_redmule_trigger_async(0);
            flex_redmule_trigger_wait(0);
        }

        flex_global_barrier_xy();
    }

    if (flex_is_dm_core()){
        flex_dma_pattern_access_west_hbm(0, 0, tile_size_byte);
        flex_dma_pattern_access_west_hbm(0, 0, tile_size_byte);
    }

    if (flex_is_first_core())
    {
        flex_redmule_trigger_async(0);
        flex_redmule_trigger_wait(0);
    }

    flex_global_barrier_xy();
    flex_timer_end();

}

void llm_mlp_inter_cluster_matmul_west_south_placement(uint32_t tile_dimension, uint32_t elem_size, uint32_t num_round_shift){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    //Initialize RedMule Paramters
    if (flex_is_first_core())
    {
        flex_redmule_set_M(0, tile_dimension);
        flex_redmule_set_N(0, tile_dimension);
        flex_redmule_set_K(0, tile_dimension);
    }

    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_round_shift-1; ++i)
    {
        if (flex_is_dm_core()){
            flex_dma_pattern_round_shift_left(0, 0, tile_size_byte);
            flex_dma_pattern_round_shift_up(0, 0, tile_size_byte);
        }

        if (flex_is_first_core())
        {
            flex_redmule_trigger_async(0);
            flex_redmule_trigger_wait(0);
        }

        flex_global_barrier_xy();
    }

    if (flex_is_dm_core()){
        flex_dma_async_pattern_access_west_hbm(0, 0, tile_size_byte);
        flex_dma_async_pattern_access_south_hbm(0, 0, tile_size_byte);
        flex_dma_async_wait_all();
    }

    if (flex_is_first_core())
    {
        flex_redmule_trigger_async(0);
        flex_redmule_trigger_wait(0);
    }

    flex_global_barrier_xy();
    flex_timer_end();

}

void llm_mlp_inter_cluster_matmul_optimize_hbm(uint32_t tile_dimension, uint32_t elem_size, uint32_t num_round_shift){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    //Initialize RedMule Paramters
    if (flex_is_first_core())
    {
        flex_redmule_set_M(0, tile_dimension);
        flex_redmule_set_N(0, tile_dimension);
        flex_redmule_set_K(0, tile_dimension);
    }

    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_round_shift; ++i)
    {
        if (flex_is_dm_core()){
            flex_dma_async_pattern_round_shift_left(0, 0, tile_size_byte);
            flex_dma_async_pattern_round_shift_up(0, 0, tile_size_byte);
            if ((flex_get_cluster_id()%ARCH_NUM_CLUSTER_X) == i)
            {
                flex_dma_async_pattern_access_west_hbm(0,0,tile_size_byte);
                flex_dma_async_pattern_access_west_hbm(0,0,tile_size_byte);
            }
            flex_dma_async_wait_all();
        }

        if (flex_is_first_core())
        {
            flex_redmule_trigger_async(0);
            flex_redmule_trigger_wait(0);
        }

        flex_global_barrier_xy();
    }

    flex_timer_end();
    
}

void llm_mlp_inter_cluster_matmul_optimize_hbm_west_south_placement(uint32_t tile_dimension, uint32_t elem_size, uint32_t num_round_shift){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    //Initialize RedMule Paramters
    if (flex_is_first_core())
    {
        flex_redmule_set_M(0, tile_dimension);
        flex_redmule_set_N(0, tile_dimension);
        flex_redmule_set_K(0, tile_dimension);
    }

    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_round_shift; ++i)
    {
        if (flex_is_dm_core()){
            flex_dma_async_pattern_round_shift_left(0, 0, tile_size_byte);
            flex_dma_async_pattern_round_shift_up(0, 0, tile_size_byte);
            if ((flex_get_cluster_id()%ARCH_NUM_CLUSTER_X) == i)
            {
                flex_dma_async_pattern_access_west_hbm(0,0,tile_size_byte);
            }
            if ((flex_get_cluster_id()/ARCH_NUM_CLUSTER_Y) == i)
            {
                flex_dma_async_pattern_access_south_hbm(0,0,tile_size_byte);
            }
            flex_dma_async_wait_all();
        }

        if (flex_is_first_core())
        {
            flex_redmule_trigger_async(0);
            flex_redmule_trigger_wait(0);
        }

        flex_global_barrier_xy();
    }

    flex_timer_end();
    
}

void llm_mlp_inter_cluster_matmul_systolic(uint32_t tile_dimension, uint32_t elem_size, uint32_t num_round_shift){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    //Initialize RedMule Paramters
    if (flex_is_first_core())
    {
        flex_redmule_set_M(0, tile_dimension);
        flex_redmule_set_N(0, tile_dimension);
        flex_redmule_set_K(0, tile_dimension);
    }

    //Benchmarking
    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_round_shift; ++i)
    {
        if (flex_is_dm_core()){
            flex_dma_pattern_systolic_shift_west_south(0,0,tile_size_byte);
        }

        if (flex_is_first_core())
        {
            flex_redmule_trigger_async(0);
            flex_redmule_trigger_wait(0);
        }

        flex_global_barrier_xy();
    }

    flex_timer_end();
    
}



/****************************
*  Testing functions below  *
****************************/

void llm_mlp_inter_cluster_matmul_pattern_only(uint32_t tile_dimension, uint32_t elem_size, uint32_t num_round_shift){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_round_shift-1; ++i)
    {
        if (flex_is_dm_core()){
            flex_dma_async_pattern_round_shift_left(0, 0, tile_size_byte);
            flex_dma_async_pattern_round_shift_up(0, 0, tile_size_byte);
            flex_dma_async_wait_all();
        }
        flex_global_barrier_xy();
    }

    if (flex_is_dm_core()){
        flex_dma_async_pattern_access_west_hbm(0,0,tile_size_byte);
        // flex_dma_async_pattern_access_west_hbm(0,0,tile_size_byte);
        flex_dma_async_wait_all();
    }

    flex_global_barrier_xy();
    flex_timer_end();
    
}

void llm_mlp_inter_cluster_matmul_optimize_hbm_pattern_only(uint32_t tile_dimension, uint32_t elem_size, uint32_t num_round_shift){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_round_shift; ++i)
    {
        if (flex_is_dm_core()){
            flex_dma_async_pattern_round_shift_left(0, 0, tile_size_byte);
            flex_dma_async_pattern_round_shift_up(0, 0, tile_size_byte);
            if ((flex_get_cluster_id()%ARCH_NUM_CLUSTER_X) == i)
            {
                flex_dma_async_pattern_access_west_hbm(0,0,tile_size_byte);
                flex_dma_async_pattern_access_west_hbm(0,0,tile_size_byte);
            }
            flex_dma_async_wait_all();
        }

        flex_global_barrier_xy();
    }

    flex_timer_end();
    
}

void llm_mlp_inter_cluster_matmul_systolic_pattern_only(uint32_t tile_dimension, uint32_t elem_size, uint32_t num_round_shift){

    uint32_t tile_size_byte = tile_dimension * tile_dimension * elem_size;

    flex_global_barrier_xy();
    flex_timer_start();

    for (int i = 0; i < num_round_shift; ++i)
    {
        if (flex_is_dm_core() || flex_is_dm2_core()){
            flex_dma_pattern_systolic_shift_west_south(0,0,tile_size_byte);
        }
        flex_global_barrier_xy();
    }

    flex_timer_end();
    
}



#endif