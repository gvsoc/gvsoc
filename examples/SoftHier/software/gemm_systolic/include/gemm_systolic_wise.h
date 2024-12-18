#ifndef _GEMM_SYSTOLIC_WISE_H_
#define _GEMM_SYSTOLIC_WISE_H_

#include <math.h>
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

typedef struct GemmSystolicInfo
{
    //General information
    uint32_t matrix_M;
    uint32_t matrix_N;
    uint32_t matrix_K;

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
    uint32_t X_offset_2;
    uint32_t W_offset_2;
    uint32_t Y_offset;

    //Iteration
    uint32_t XW_tile_length;
    uint32_t Z_tile_on_row;
    uint32_t Z_tile_on_col;
    uint32_t Z_tile_all;
    uint32_t systolic_delay;

    uint32_t total_iter;

    //Current Actions
    uint32_t dma_runing;
    uint32_t redmule_runing;

    //Recorded Actions
    uint32_t use_dma1;
    uint32_t dma1_src;
    uint32_t dma1_dst;
    uint32_t dma1_size;

    uint32_t use_dma2;
    uint32_t dma2_src;
    uint32_t dma2_dst;
    uint32_t dma2_size;

    uint32_t use_redmule;
    uint32_t redmule_x;
    uint32_t redmule_w;
    uint32_t redmule_y;
}GemmSystolicInfo;

GemmSystolicInfo gemm_systolic_wise_analysis(
    uint32_t M_size, uint32_t N_size, uint32_t K_size, uint32_t elem_size,
    uint32_t tile_dimension_M, uint32_t tile_dimension_N, uint32_t tile_dimension_K){
    GemmSystolicInfo info;
    info.matrix_M = M_size;
    info.matrix_N = N_size;
    info.matrix_K = K_size;
    info.tile_dimension_M = tile_dimension_M;
    info.tile_dimension_N = tile_dimension_N;
    info.tile_dimension_K = tile_dimension_K;
    info.elem_size = elem_size;
    info.tile_size_byte_X = info.tile_dimension_M * info.tile_dimension_N * elem_size;
    info.tile_size_byte_W = info.tile_dimension_N * info.tile_dimension_K * elem_size;
    info.tile_size_byte_Y = info.tile_dimension_M * info.tile_dimension_K * elem_size;

    info.X_offset_1 = 0;
    info.W_offset_1 = 1 * info.tile_size_byte_X;
    info.X_offset_2 = 2 * info.tile_size_byte_W;
    info.W_offset_2 = 3 * info.tile_size_byte_X;
    info.Y_offset   = 4 * info.tile_size_byte_W;

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

    info.dma_runing = 0;
    info.redmule_runing = 0;

    return info;
}


void gemm_systolic_wise_compute_dma_access(GemmSystolicInfo * info, uint32_t iter){

    //Set defualt number
    info->use_dma1 = 0;
    info->use_dma2 = 0;

    //Determine DMA actions
    if ((iter >= info->systolic_delay) && (iter < (info->Z_tile_all * (info->XW_tile_length + 1) + info->systolic_delay)))
    {
        uint32_t eff_iter = iter - info->systolic_delay;
        uint32_t sub_iter = eff_iter%(info->XW_tile_length + 1);
        uint32_t st_count = eff_iter/(info->XW_tile_length + 1);
        uint32_t xw_count = st_count * info->XW_tile_length + sub_iter - 1;
        FlexPosition pos = get_pos(flex_get_cluster_id());

        if (sub_iter == 0)
        {
            if (st_count != 0)
            {
                info->use_dma1 = 1;
                info->dma1_dst = hbm_south(pos.x,0) + (info->matrix_N * info->matrix_K * info->elem_size / ARCH_NUM_CLUSTER_X) + (st_count - 1) * ARCH_NUM_CLUSTER_Y * info->tile_size_byte_Y + pos.y * info->tile_size_byte_Y;
                info->dma1_src = local(info->Y_offset);
                info->dma1_size = info->tile_size_byte_Y;
            }
        } else {
            info->use_dma1 = 1;
            info->use_dma2 = 1;
            info->dma1_size = info->tile_size_byte_X;
            info->dma2_size = info->tile_size_byte_W;

            uint32_t local_x = (xw_count%2 == 0)? info->X_offset_1 : info->X_offset_2;
            uint32_t local_w = (xw_count%2 == 0)? info->W_offset_1 : info->W_offset_2;

            //XW tile transfering
            if(pos.x == 0){
                /* clusters at west edge hbm transfer*/
                info->dma1_dst = local(local_x);
                info->dma1_src = hbm_west(pos.y,0) + xw_count * info->tile_size_byte_X;
            } else {
                /* clusters on-chip transfer*/
                info->dma1_dst = local(local_x);
                info->dma1_src = remote_pos(left_pos(pos),local_x);
            }

            if (pos.y == 0)
            {
                /* clusters at south edge hbm transfer*/
                info->dma2_dst = local(local_w);
                info->dma2_src = hbm_south(pos.x,0) + xw_count * info->tile_size_byte_W;
            } else {
                /* clusters on-chip transfer*/
                info->dma2_dst = local(local_w);
                info->dma2_src = remote_pos(bottom_pos(pos),local_w);
            }
        }
    }
}

void gemm_systolic_wise_compute_redmule_action(GemmSystolicInfo * info, uint32_t iter){

    //Set defualt number
    info->use_redmule = 0;

    //Determine RedMule actions
    if ((iter >= (info->systolic_delay + 1)) && (iter < (info->Z_tile_all * (info->XW_tile_length + 1) + info->systolic_delay + 1)))
    {
        uint32_t eff_iter = iter - info->systolic_delay - 1;
        uint32_t sub_iter = eff_iter%(info->XW_tile_length + 1);
        uint32_t st_count = eff_iter/(info->XW_tile_length + 1);
        uint32_t xw_count = st_count * info->XW_tile_length + sub_iter - 1;
        if (sub_iter != 0)
        {
            info->use_redmule = 1;
            info->redmule_x = (xw_count%2 == 0)? info->X_offset_1 : info->X_offset_2;
            info->redmule_w = (xw_count%2 == 0)? info->W_offset_1 : info->W_offset_2;
            info->redmule_y = info->Y_offset;
        }
    }
}


void gemm_systolic_wise(
    uint32_t M_size, uint32_t N_size, uint32_t K_size, uint32_t elem_size,
    uint32_t tile_dimension_M, uint32_t tile_dimension_N, uint32_t tile_dimension_K){

    flex_global_barrier_xy();
    uint32_t CID = flex_get_cluster_id();
    GemmSystolicInfo info = gemm_systolic_wise_analysis(M_size, N_size, K_size, elem_size,tile_dimension_M,tile_dimension_N,tile_dimension_K);

    if (flex_is_first_core())
    {
        //Initialize RedMule Paramters
        flex_redmule_config(info.tile_dimension_M, info.tile_dimension_N, info.tile_dimension_K);
        //Pre-Compute RedMule actions for the first iteration
        gemm_systolic_wise_compute_redmule_action(&info, 0);
    }

    if (flex_is_dm_core())
    {
        //Pre-Compute DMA actions for the first iteration
        gemm_systolic_wise_compute_dma_access(&info, 0);
    }

    flex_global_barrier_xy();
 
    for (int i = 0; i < info.total_iter; ++i)
    {
        if (flex_is_first_core())
        {
            //Asynchronizly execute redmule actions
            if (info.use_redmule) flex_redmule_trigger(info.redmule_x, info.redmule_w, info.redmule_y, REDMULE_NONE_16);
            info.redmule_runing = info.use_redmule;

            //Compute for next redmule actions
            gemm_systolic_wise_compute_redmule_action(&info, i+1);

            //Wait for redmule done
            if (info.redmule_runing) flex_redmule_wait();
        }

        if (flex_is_dm_core())
        {
            //Asynchronizly execute idma actions
            if (info.use_dma1) flex_dma_async_1d(info.dma1_dst, info.dma1_src, info.dma1_size);
            if (info.use_dma2) flex_dma_async_1d(info.dma2_dst, info.dma2_src, info.dma2_size);
            info.dma_runing = info.use_dma1 | info.use_dma2;

            //Compute for next idma actions
            gemm_systolic_wise_compute_dma_access(&info, i+1);

            //Wait for idma done
            if (info.dma_runing) flex_dma_async_wait_all();
        }

        //Global synchronization
        flex_global_barrier_xy();
    }
}


#endif