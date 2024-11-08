#ifndef _GEMM_SYSTOLIC_WISE_H_
#define _GEMM_SYSTOLIC_WISE_H_

#include <math.h>
#include "snrt.h"
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

typedef struct GemmSystolicInfo
{
    //Tile Information
    uint32_t tile_dimension_M;
    uint32_t tile_dimension_N;
    uint32_t tile_dimension_K;
    uint32_t elem_size;
    uint32_t tile_size_byte_X;
    uint32_t tile_size_byte_W;
    uint32_t tile_size_byte_Y;

    //L1 addr
    uint32_t X_offset_1;
    uint32_t W_offset_1;
    uint32_t Y_offset_1;
    uint32_t X_offset_2;
    uint32_t W_offset_2;
    uint32_t Y_offset_2;

    //Iteration
    uint32_t XW_tile_length;
    uint32_t Z_tile_on_row;
    uint32_t Z_tile_on_col;
    uint32_t Z_tile_all;
    uint32_t systolic_delay;

    uint32_t total_iter;
}GemmSystolicInfo;

GemmSystolicInfo gemm_systolic_wise_analysis(
    uint32_t M_size, uint32_t N_size, uint32_t K_size, uint32_t elem_size,
    uint32_t tile_dimension_M, uint32_t tile_dimension_N, uint32_t tile_dimension_K){
    GemmSystolicInfo info;
    info.tile_dimension_M = tile_dimension_M;
    info.tile_dimension_N = tile_dimension_N;
    info.tile_dimension_K = tile_dimension_K;
    info.elem_size = elem_size;
    info.tile_size_byte_X = info.tile_dimension_M * info.tile_dimension_N * elem_size;
    info.tile_size_byte_W = info.tile_dimension_N * info.tile_dimension_K * elem_size;
    info.tile_size_byte_Y = info.tile_dimension_M * info.tile_dimension_K * elem_size;

    info.X_offset_1 = 0;
    info.W_offset_1 = 1 * info.tile_size_byte_X;
    info.Y_offset_1 = 2 * info.tile_size_byte_W;
    info.X_offset_2 = 3 * info.tile_size_byte_Y;
    info.W_offset_2 = 4 * info.tile_size_byte_X;
    info.Y_offset_2 = 5 * info.tile_size_byte_W;

    uint32_t M_tile = (M_size + info.tile_dimension_M - 1)/info.tile_dimension_M;
    uint32_t N_tile = (N_size + info.tile_dimension_N - 1)/info.tile_dimension_N;
    uint32_t K_tile = (K_size + info.tile_dimension_K - 1)/info.tile_dimension_K;

    FlexPosition pos = get_pos(flex_get_cluster_id());
    info.Z_tile_on_row = 0;
    while((info.Z_tile_on_row * ARCH_NUM_CLUSTER_X + pos.x) < K_tile){
        info.Z_tile_on_row ++;
    }
    info.Z_tile_on_col = 0;
    while((info.Z_tile_on_col * ARCH_NUM_CLUSTER_Y + pos.y) < M_tile){
        info.Z_tile_on_col ++;
    }
    info.XW_tile_length = N_tile;
    info.Z_tile_all = info.Z_tile_on_row * info.Z_tile_on_col;
    info.systolic_delay = pos.x + pos.y;
    info.total_iter = ((K_tile + ARCH_NUM_CLUSTER_X -1)/ARCH_NUM_CLUSTER_X) * ((M_tile + ARCH_NUM_CLUSTER_Y - 1)/ARCH_NUM_CLUSTER_Y) * (N_tile + 1) + ARCH_NUM_CLUSTER_X + ARCH_NUM_CLUSTER_Y;

    return info;
}

void gemm_systolic_wise_dma_access(GemmSystolicInfo info, uint32_t iter){

    if ((iter >= info.systolic_delay) && (iter < (info.Z_tile_all * (info.XW_tile_length + 1) + info.systolic_delay)))
    {
        uint32_t eff_iter = iter - info.systolic_delay;
        uint32_t sub_iter = eff_iter%(info.XW_tile_length + 1);
        uint32_t remote_X = 0;
        uint32_t remote_W = 0;
        uint32_t remote_Y = 0;
        uint32_t remote_Z = 0;
        uint32_t local_X = 0;
        uint32_t local_W = 0;
        uint32_t local_Y = 0;
        uint32_t local_Z = 0;
        FlexPosition pos = get_pos(flex_get_cluster_id());

        if (sub_iter == 0)
        {
            //YZ tile transfering
            flex_dma_async_1d(local(local_Y),hbm_south(pos.x,remote_Y), info.tile_size_byte_Y);
            flex_dma_async_1d(local(local_Z),hbm_west(pos.y,remote_Z), info.tile_size_byte_Y);
            flex_dma_async_wait_all();
        } else {
            //XW tile transfering
            if(pos.x == 0){
                /* clusters at west edge hbm transfer*/
                flex_dma_async_1d(local(local_X),hbm_west(pos.y,remote_X), info.tile_size_byte_X);
            } else {
                /* clusters on-chip transfer*/
                flex_dma_async_1d(local(local_X),remote_pos(left_pos(pos),remote_X), info.tile_size_byte_X);
            }

            if (pos.y == 0)
            {
                /* clusters at south edge hbm transfer*/
                flex_dma_async_1d(local(local_W),hbm_south(pos.x,remote_W), info.tile_size_byte_W);
            } else {
                /* clusters on-chip transfer*/
                flex_dma_async_1d(local(local_W),remote_pos(bottom_pos(pos),remote_W), info.tile_size_byte_W);
            }
            flex_dma_async_wait_all();
        }
    }
}

void gemm_systolic_wise_redmule(GemmSystolicInfo info, uint32_t iter){

    if ((iter >= (info.systolic_delay + 1)) && (iter < (info.Z_tile_all * (info.XW_tile_length + 1) + info.systolic_delay + 1)))
    {
        uint32_t eff_iter = iter - info.systolic_delay - 1;
        uint32_t sub_iter = eff_iter%(info.XW_tile_length + 1);
        if (sub_iter != 0)
        {
            flex_redmule_trigger(0, 0, 0, REDMULE_INT_16);
            flex_redmule_wait();
        }
    }
}

void gemm_systolic_wise(
    uint32_t M_size, uint32_t N_size, uint32_t K_size, uint32_t elem_size,
    uint32_t tile_dimension_M, uint32_t tile_dimension_N, uint32_t tile_dimension_K){

    flex_global_barrier_xy();
    uint32_t CID = flex_get_cluster_id();
    GemmSystolicInfo info = gemm_systolic_wise_analysis(M_size, N_size, K_size, elem_size,tile_dimension_M,tile_dimension_N,tile_dimension_K);

    //Initialize RedMule Paramters
    if (flex_is_first_core())
    {
        flex_redmule_config(info.tile_dimension_M, info.tile_dimension_N, info.tile_dimension_K);
        if (CID == 0)
        {
            flex_log(info.total_iter);
        }
    }

    flex_global_barrier_xy();

    for (int i = 0; i < info.total_iter; ++i)
    {
        if (flex_is_dm_core()){
            gemm_systolic_wise_dma_access(info, i);
        }

        if (flex_is_first_core())
        {
            gemm_systolic_wise_redmule(info, i);
        }
        flex_global_barrier_xy();
    }
}


#endif