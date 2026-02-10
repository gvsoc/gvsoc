#ifndef _GEMM_SYSTOLIC_WISE_H_
#define _GEMM_SYSTOLIC_WISE_H_

#include <math.h>
#include "flex_runtime_3d.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern_3d.h"

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

    // 3D Added tile info for layer-partitioned chunks
    uint32_t tile_dimension_K_3dchunk;
    uint32_t tile_size_byte_W_3dchunk;
    uint32_t tile_size_byte_Y_3dchunk;

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

    uint32_t use_sync_dma;

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

    // When 3D-chunked, each cluster in a stack gets an `ARCH_NUM_CLUSTER_Z`'th
    // of a chunk of W and Y, respectively (all clusters get "full 2D" copies of X).
    // We thus round UP chunks: the remainder in the topmost layer may be garbage, but that's fine.
    info.tile_dimension_K_3dchunk = (tile_dimension_K + ARCH_NUM_CLUSTER_Z - 1) / ARCH_NUM_CLUSTER_Z;
    info.tile_size_byte_W_3dchunk = info.tile_dimension_N * info.tile_dimension_K_3dchunk * elem_size / ARCH_NUM_CLUSTER_Z;
    info.tile_size_byte_Y_3dchunk = info.tile_dimension_N * info.tile_dimension_K_3dchunk * elem_size / ARCH_NUM_CLUSTER_Z;

    // We still allocate full-sized buffers for W, as edge clustes must pass up some
    // portion of W (The bottom clusters must store the entire thing).
    // Note that we could, however, accomodate potentially larger tiles, as the maximum
    // size of our Y tiles is reduced in all clusters equally.
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
    // We can just add the Z position here, as upper-layer clusters are
    // additionally "behind" by their height vs. the 2D case.
    info.systolic_delay = pos.x + pos.y + pos.z;
    // The number of "whole-chunk" (not 3D partitioned) iteration stays the same by construction,
    // Except that we also add `ARCH_NUM_CLUSTER_Z` to compensate for larger systolic delays.
    info.total_iter = ((K_tile + ARCH_NUM_CLUSTER_X -1)/ARCH_NUM_CLUSTER_X) * ((M_tile + ARCH_NUM_CLUSTER_Y - 1)/ARCH_NUM_CLUSTER_Y) * (N_tile + 1) + ARCH_NUM_CLUSTER_X + ARCH_NUM_CLUSTER_Y + ARCH_NUM_CLUSTER_Z;

    info.dma_runing = 0;
    info.redmule_runing = 0;

    return info;
}


void gemm_systolic_wise_compute_dma_access(GemmSystolicInfo * info, uint32_t iter){

    //Set defualt number
    info->use_dma1 = 0;
    info->use_dma2 = 0;
    info->use_sync_dma = 0;

    //Determine DMA actions
    if ((iter >= info->systolic_delay) && (iter < (info->Z_tile_all * (info->XW_tile_length + 1) + info->systolic_delay)))
    {
        uint32_t eff_iter = iter - info->systolic_delay;
        uint32_t sub_iter = eff_iter%(info->XW_tile_length + 1);
        uint32_t st_count = eff_iter/(info->XW_tile_length + 1);
        uint32_t xw_count = (sub_iter < 1)? st_count * info->XW_tile_length : st_count * info->XW_tile_length + sub_iter - 1;
        FlexPosition pos = get_pos(flex_get_cluster_id());

        if (sub_iter == 1)
        {
            if (st_count != 0)
            {
                // We write an initial value of 0 for the block to HBM and to ourselves.
                // We account for the per-layer Z offset in the HBM destination address.
                info->use_sync_dma = 1;
                info->use_dma1 = 1;
                info->dma1_dst = hbm_south(pos.x,0) +
                    (info->matrix_N * info->matrix_K * info->elem_size / ARCH_NUM_CLUSTER_X) +
                    (st_count - 1) * ARCH_NUM_CLUSTER_Y * info->tile_size_byte_Y +
                    pos.y * info->tile_size_byte_Y + 
                    pos.z * info->tile_size_byte_Y_3dchunk;
                info->dma1_src = local(info->Y_offset);
                info->dma1_size = info->tile_size_byte_Y_3dchunk;
                info->use_dma2 = 1;
                info->dma2_dst = local(info->Y_offset);
                info->dma2_src = zomem(0);
                info->dma2_size = info->tile_size_byte_Y_3dchunk;
            }
        // For regular subiterations, just bring in new double-buffered X or W, respectively.
        } else {
            info->use_dma1 = 1;
            info->use_dma2 = 1;
            info->dma1_size = info->tile_size_byte_X;
            info->dma2_size = info->tile_size_byte_W_3dchunk;

            uint32_t local_x = (xw_count%2 == 0)? info->X_offset_1 : info->X_offset_2;
            uint32_t local_w = (xw_count%2 == 0)? info->W_offset_1 : info->W_offset_2;

            //XW tile transfering
            // Get X from west HBM if at left edge (x = 0),
            // otherwise from cluster at left edge (no timing guard for this??)
            if(pos.x == 0){
                if (pos.z == 0) {
                    // clusters at base: get all values for stack from west-edge HBM.
                    // Forward them all as-is to above clusters.
                    info->dma1_dst = local(local_x);
                    info->dma1_src = hbm_west(pos.y,0) + xw_count * info->tile_size_byte_X;
                } else {
                    info->dma1_dst = local(local_x);
                    info->dma1_src = remote_pos(zminus_pos(pos),local_x);
                }
            } else {
                /* clusters on-chip transfer*/
                info->dma1_dst = local(local_x);
                info->dma1_src = remote_pos(left_pos(pos),local_x);
            }

            // Get W from south HBM if at bottom edge (y = 0),
            // otherwise from cluster at bottom edge (no timing guard for this??)
            if (pos.y == 0) {
                if (pos.z == 0) {
                    // clusters at base: get all weights for stack from south-edge HBM.
                    // bring in entire chunk size for all clusters above you.
                    info->dma2_dst = local(local_w);
                    info->dma2_src = hbm_south(pos.x,0) + xw_count * info->tile_size_byte_W;
                    info->dma2_size = info->tile_size_byte_W;
                } else {
                    // upper edge (z > 0) clusters pull their data from below.
                    // They only need to fetch increasingly small blocks to provide to those above.
                    info->dma2_dst = local(local_w);
                    info->dma2_src = remote_pos(zminus_pos(pos),local_w) + info->tile_size_byte_W_3dchunk;
                    info->dma2_size = info->tile_size_byte_W_3dchunk * (ARCH_NUM_CLUSTER_Z - pos.z);
                }

            } else {
                // clusters on-chip transfer.
                // Since we up-copy the back part of the vector, the forwarded part
                // Stays exactly at `local(local_w)`, simplifying processing.
                info->dma2_dst = local(local_w);
                info->dma2_src = remote_pos(bottom_pos(pos),local_w);
            }
        }
    }

    //Final Store
    if (iter == (info->Z_tile_all * (info->XW_tile_length + 1) + info->systolic_delay + 1))
    {
        uint32_t eff_iter = iter - info->systolic_delay;
        uint32_t st_count = eff_iter/(info->XW_tile_length + 1);
        FlexPosition pos = get_pos(flex_get_cluster_id());

        info->use_dma1 = 1;
        info->dma1_dst = hbm_south(pos.x,0) +
            (info->matrix_N * info->matrix_K * info->elem_size / ARCH_NUM_CLUSTER_X) +
            (st_count - 1) * ARCH_NUM_CLUSTER_Y * info->tile_size_byte_Y +
            pos.y * info->tile_size_byte_Y +
            pos.z * info->tile_size_byte_Y_3dchunk;
        info->dma1_src = local(info->Y_offset);
        info->dma1_size = info->tile_size_byte_Y_3dchunk;
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
        uint32_t xw_count = (sub_iter < 1)? st_count * info->XW_tile_length : st_count * info->XW_tile_length + sub_iter - 1;
        if (sub_iter != 1)
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

    flex_global_barrier_xyz();
    GemmSystolicInfo info = gemm_systolic_wise_analysis(M_size, N_size, K_size, elem_size,tile_dimension_M,tile_dimension_N,tile_dimension_K);

    if (flex_is_first_core())
    {
        //Initialize RedMule Paramters
        flex_redmule_config(info.tile_dimension_M, info.tile_dimension_N, info.tile_dimension_K_3dchunk);
        //Pre-Compute RedMule actions for the first iteration
        gemm_systolic_wise_compute_redmule_action(&info, 0);
    }

    if (flex_is_dm_core())
    {
        //Pre-Compute DMA actions for the first iteration
        gemm_systolic_wise_compute_dma_access(&info, 0);
    }

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xyz();
 
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
            if (info.use_sync_dma)
            {
                //Synchronizly execute idma actions
                if (info.use_dma1) {flex_dma_async_1d(info.dma1_dst, info.dma1_src, info.dma1_size); flex_dma_async_wait_all();}
                if (info.use_dma2) {flex_dma_async_1d(info.dma2_dst, info.dma2_src, info.dma2_size); flex_dma_async_wait_all();}
                //Compute for next idma actions
                gemm_systolic_wise_compute_dma_access(&info, i+1);
            } else {
                //Asynchronizly execute idma actions
                if (info.use_dma1) flex_dma_async_1d(info.dma1_dst, info.dma1_src, info.dma1_size);
                if (info.use_dma2) flex_dma_async_1d(info.dma2_dst, info.dma2_src, info.dma2_size);
                info.dma_runing = info.use_dma1 | info.use_dma2;

                //Compute for next idma actions
                gemm_systolic_wise_compute_dma_access(&info, i+1);

                //Wait for idma done
                if (info.dma_runing) flex_dma_async_wait_all();
            }
        }

        //Global synchronization
        flex_global_barrier_xyz();
    }
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
}


#endif
