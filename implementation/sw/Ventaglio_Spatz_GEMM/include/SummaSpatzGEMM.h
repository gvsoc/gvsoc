// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: SHL-0.51
//
// Based on: SummaGEMM.h by Chi Zhang <chizhang@iis.ee.ethz.ch>
// Modified by: Bowen Wang, ETH Zurich
//
// SUMMA GEMM using Spatz vector processor instead of RedMule.
// The SUMMA dataflow (tiling, DMA, barriers, double-buffering, groups)
// is identical to the original. Only the local tile compute is replaced:
//   RedMule: flex_redmule_config() + flex_redmule_trigger() + flex_redmule_wait()
//   Spatz:   spatz_tile_gemm() — synchronous, no separate wait needed.

#ifndef _SUMMA_SPATZ_GEMM_H_
#define _SUMMA_SPATZ_GEMM_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "spatz_tile_gemm.h"
#include "spatz_tile_gemv.h"
#include "spatz_gemm.h"

typedef struct SummaGEMMInfo
{
    uint64_t                    X_address;
    uint64_t                    W_address;
    uint64_t                    Z_address;
    uint32_t                    M_size;
    uint32_t                    N_size;
    uint32_t                    K_size;
    uint32_t                    M_tile;
    uint32_t                    N_tile;
    uint32_t                    K_tile;
    uint32_t                    group_reduction;
    uint32_t                    group_splitK;
    uint32_t                    group_splitN;
    uint32_t                    summa_group_x;
    uint32_t                    summa_group_y;
    uint32_t                    summa_groups;

    FlexPosition                cluster_pos;
    uint32_t                    cluster_global_id;
    uint32_t                    cluster_active;

    GridSyncGroupInfo           group;
    uint32_t                    cluster_in_group_id;
    uint32_t                    cluster_in_group_id_x;
    uint32_t                    cluster_in_group_id_y;
    uint32_t                    cluster_for_rowwise;
    uint32_t                    cluster_for_colwise;

    uint32_t                    M_iter;
    uint32_t                    N_iter;
    uint32_t                    K_iter;

    uint32_t                    store_recorded;
    uint32_t                    store_m;
    uint32_t                    store_n;
    uint32_t                    store_step;
    uint32_t                    store_id;
    uint32_t                    store_id_offset;
    uint32_t                    store_active;

    uint64_t                    X_tile_base;
    uint64_t                    W_tile_base;
    uint64_t                    Z_tile_base;
    uint64_t                    X_tile_M_iter_offset;
    uint64_t                    X_tile_N_iter_offset;
    uint64_t                    X_tile_K_iter_offset;
    uint64_t                    W_tile_M_iter_offset;
    uint64_t                    W_tile_N_iter_offset;
    uint64_t                    W_tile_K_iter_offset;
    uint64_t                    Z_tile_M_iter_offset;
    uint64_t                    Z_tile_N_iter_offset;

    uint32_t                    L1_X1;
    uint32_t                    L1_W1;
    uint32_t                    L1_Z1;
    uint32_t                    L1_X2;
    uint32_t                    L1_W2;
    uint32_t                    L1_Z2;
    uint32_t                    L1_AREA;
    uint32_t                    L1_X_size;
    uint32_t                    L1_W_size;
    uint32_t                    L1_Z_size;
}SummaGEMMInfo;

SummaGEMMInfo SummaGEMMAnaylze(
    uint64_t X_address, uint64_t W_address, uint64_t Z_address,
    uint32_t M_size, uint32_t N_size, uint32_t K_size,
    uint32_t M_tile, uint32_t N_tile, uint32_t K_tile,
    uint32_t group_x, uint32_t group_y,
    uint32_t num_group, uint32_t group_reduction,
    uint32_t group_splitK, uint32_t group_splitN,
    uint32_t X_address_group_gap, uint32_t W_address_group_gap, uint32_t Z_address_group_gap)
{
    SummaGEMMInfo info;

    info.X_address              = X_address;
    info.W_address              = W_address;
    info.Z_address              = Z_address;
    info.M_size                 = M_size;
    info.N_size                 = N_size;
    info.K_size                 = K_size;
    info.M_tile                 = M_tile;
    info.N_tile                 = N_tile;
    info.K_tile                 = K_tile;
    info.group_reduction        = group_reduction;
    info.group_splitK           = group_splitK;
    info.group_splitN           = group_splitN;
    info.summa_group_x          = group_x;
    info.summa_group_y          = group_y;
    info.summa_groups           = num_group;

    FlexPosition pos            = get_pos(flex_get_cluster_id());
    info.cluster_pos            = pos;
    info.cluster_global_id      = flex_get_cluster_id();
    info.group                  = grid_sync_group_init(info.summa_group_x, info.summa_group_y);
    info.cluster_in_group_id_x  = pos.x % info.group.grid_x_dim;
    info.cluster_in_group_id_y  = pos.y % info.group.grid_y_dim;
    info.cluster_in_group_id    = info.cluster_in_group_id_x + info.group.this_grid_cluster_num_x * info.cluster_in_group_id_y;
    info.cluster_active         = info.group.this_grid_id < info.summa_groups ? 1 : 0;
    info.cluster_for_rowwise    = ((info.cluster_in_group_id_x % info.group.grid_y_dim) == (info.cluster_in_group_id_y % info.group.grid_x_dim) && (info.cluster_in_group_id_x == (pos.y % info.group.grid_x_dim)))? 1 : 0;
    info.cluster_for_colwise    = ((info.cluster_in_group_id_x % info.group.grid_y_dim) == (info.cluster_in_group_id_y % info.group.grid_x_dim) && (info.cluster_in_group_id_y == (pos.x % info.group.grid_y_dim)))? 1 : 0;

    info.M_iter                 = (M_size + info.summa_group_y * M_tile - 1) / (info.summa_group_y * M_tile);
    info.N_iter                 = info.group_splitN? (((N_size / info.summa_groups) + info.summa_group_x * N_tile - 1) / (info.summa_group_x * N_tile)) : (N_size + info.summa_group_x * N_tile - 1) / (info.summa_group_x * N_tile);
    info.K_iter                 = info.group_splitK? (((K_size / (info.summa_groups * info.group_splitK)) + K_tile - 1) / K_tile) : ((K_size + K_tile - 1) / K_tile);
    info.store_recorded         = 0;
    info.store_m                = 0;
    info.store_n                = 0;
    info.store_step             = (info.summa_group_x + info.K_iter - 1) / info.K_iter;
    info.store_id               = info.summa_group_x;
    info.store_id_offset        = pos.y % info.summa_group_x;
    info.store_active           = (info.group_reduction == 0) ? 1 : (info.group.this_grid_id == 0)? 1 : 0;

    info.X_tile_base            = X_address + X_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_y * M_tile * K_size * DATA_TYPE_BYTE;
    info.W_tile_base            = W_address + W_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_x * N_tile * DATA_TYPE_BYTE;
    info.Z_tile_base            = Z_address + Z_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_y * M_tile * N_size * DATA_TYPE_BYTE + info.cluster_in_group_id_x * N_tile * DATA_TYPE_BYTE;

    info.X_tile_M_iter_offset   = info.summa_group_x * M_tile * K_size * DATA_TYPE_BYTE;
    info.X_tile_N_iter_offset   = 0;
    info.X_tile_K_iter_offset   = K_tile * DATA_TYPE_BYTE;
    info.W_tile_M_iter_offset   = 0;
    info.W_tile_N_iter_offset   = info.summa_group_x * N_tile * DATA_TYPE_BYTE;
    info.W_tile_K_iter_offset   = K_tile * N_size * DATA_TYPE_BYTE;
    info.Z_tile_M_iter_offset   = info.summa_group_y * M_tile * N_size * DATA_TYPE_BYTE;
    info.Z_tile_N_iter_offset   = info.summa_group_x * N_tile * DATA_TYPE_BYTE;

    info.L1_X1                  = local(0);
    info.L1_W1                  = info.L1_X1 + M_tile * K_tile * DATA_TYPE_BYTE;
    info.L1_Z1                  = info.L1_W1 + N_tile * K_tile * DATA_TYPE_BYTE;
    info.L1_X2                  = info.L1_Z1 + M_tile * N_tile * DATA_TYPE_BYTE;
    info.L1_W2                  = info.L1_X2 + M_tile * K_tile * DATA_TYPE_BYTE;
    info.L1_Z2                  = info.L1_W2 + N_tile * K_tile * DATA_TYPE_BYTE;
    info.L1_AREA                = info.L1_Z2 + M_tile * N_tile * DATA_TYPE_BYTE;
    info.L1_X_size              = M_tile * K_tile * DATA_TYPE_BYTE;
    info.L1_W_size              = N_tile * K_tile * DATA_TYPE_BYTE;
    info.L1_Z_size              = M_tile * N_tile * DATA_TYPE_BYTE;

    return info;
}

void SummaGEMMRun(SummaGEMMInfo * info)
{
    flex_global_barrier_xy();

    if (info->cluster_active)
    {
        uint32_t DMA_L1_Z = info->L1_Z2;
        uint32_t COMPUTE_L1_Z = info->L1_Z1;

        // Initialize Z buffers to zero
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(info->L1_Z1, zomem(0), info->L1_Z_size);
            flex_dma_async_1d(info->L1_Z2, zomem(0), info->L1_Z_size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        for (int m = 0; m < info->M_iter; ++m)
        {
            for (int n = 0; n < info->N_iter; ++n)
            {
                // Prefetch first tiles
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        flex_dma_async_2d(
                            info->L1_X1,
                            info->X_tile_base + m * info->X_tile_M_iter_offset + n * info->X_tile_N_iter_offset,
                            info->K_tile * DATA_TYPE_BYTE,
                            info->K_tile * DATA_TYPE_BYTE,
                            info->K_size * DATA_TYPE_BYTE,
                            info->M_tile);
                        flex_dma_async_wait_all();
                        if (info->summa_group_x > 1)
                        {
                            flex_dma_async_broadcast(
                                info->L1_X1, info->L1_X1, info->L1_X_size,
                                info->group.wakeup_row_mask, (ARCH_NUM_CLUSTER_Y - 1));
                            flex_dma_async_wait_all();
                        }
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        flex_dma_async_2d(
                            info->L1_W1,
                            info->W_tile_base + m * info->W_tile_M_iter_offset + n * info->W_tile_N_iter_offset,
                            info->N_tile * DATA_TYPE_BYTE,
                            info->N_tile * DATA_TYPE_BYTE,
                            info->N_size * DATA_TYPE_BYTE,
                            info->K_tile);
                        flex_dma_async_wait_all();
                        if (info->summa_group_y > 1)
                        {
                            flex_dma_async_broadcast(
                                info->L1_W1, info->L1_W1, info->L1_W_size,
                                (ARCH_NUM_CLUSTER_X - 1), info->group.wakeup_col_mask);
                            flex_dma_async_wait_all();
                        }
                    }
                }

                // Double-buffered K-iteration
                for (int k = 1; k <= info->K_iter; ++k)
                {
                    uint32_t DMA_L1_X     = (k % 2 == 1)? info->L1_X2 : info->L1_X1;
                    uint32_t DMA_L1_W     = (k % 2 == 1)? info->L1_W2 : info->L1_W1;
                    uint32_t COMPUTE_L1_X = (k % 2 == 1)? info->L1_X1 : info->L1_X2;
                    uint32_t COMPUTE_L1_W = (k % 2 == 1)? info->L1_W1 : info->L1_W2;

                    grid_sync_group_barrier_xy(&(info->group));
                    // No redmule_wait needed — Spatz compute is synchronous
                    flex_intra_cluster_sync();

                    // DM core: prefetch next K tile + store previous Z
                    if (flex_is_dm_core())
                    {
                        if (k < info->K_iter)
                        {
                            if (info->cluster_for_rowwise == 1)
                            {
                                flex_dma_async_2d(
                                    DMA_L1_X,
                                    info->X_tile_base + m * info->X_tile_M_iter_offset + n * info->X_tile_N_iter_offset + k * info->X_tile_K_iter_offset,
                                    info->K_tile * DATA_TYPE_BYTE,
                                    info->K_tile * DATA_TYPE_BYTE,
                                    info->K_size * DATA_TYPE_BYTE,
                                    info->M_tile);
                                flex_dma_async_wait_all();
                                if (info->summa_group_x > 1)
                                {
                                    flex_dma_async_broadcast(
                                        DMA_L1_X, DMA_L1_X, info->L1_X_size,
                                        info->group.wakeup_row_mask, (ARCH_NUM_CLUSTER_Y - 1));
                                    flex_dma_async_wait_all();
                                }
                            }
                            if (info->cluster_for_colwise == 1)
                            {
                                flex_dma_async_2d(
                                    DMA_L1_W,
                                    info->W_tile_base + m * info->W_tile_M_iter_offset + n * info->W_tile_N_iter_offset + k * info->W_tile_K_iter_offset,
                                    info->N_tile * DATA_TYPE_BYTE,
                                    info->N_tile * DATA_TYPE_BYTE,
                                    info->N_size * DATA_TYPE_BYTE,
                                    info->K_tile);
                                flex_dma_async_wait_all();
                                if (info->summa_group_y > 1)
                                {
                                    flex_dma_async_broadcast(
                                        DMA_L1_W, DMA_L1_W, info->L1_W_size,
                                        (ARCH_NUM_CLUSTER_X - 1), info->group.wakeup_col_mask);
                                    flex_dma_async_wait_all();
                                }
                            }
                        }

                        // Store previous Z tile if needed
                        if (info->store_recorded == 1 && info->store_active == 1)
                        {
                            uint32_t start_id = (info->store_id < info->store_step)? 0 : info->store_id - info->store_step;
                            uint32_t bid = (start_id + info->store_id_offset) % info->summa_group_x;
                            uint32_t eid = (info->store_id + info->store_id_offset) % info->summa_group_x;
                            if (((info->cluster_in_group_id_x >= bid && info->cluster_in_group_id_x < eid) && eid > bid) || \
                                ((info->cluster_in_group_id_x >= bid || info->cluster_in_group_id_x < eid) && eid <= bid))
                            {
                                if (info->group_reduction == 1)
                                {
                                    flex_dma_async_reduction(
                                        DMA_L1_Z, DMA_L1_Z, info->L1_Z_size,
                                        COLLECTIVE_REDSUM_TYPE,
                                        ~info->group.wakeup_row_mask,
                                        ~info->group.wakeup_col_mask);
                                    flex_dma_async_wait_all();
                                }
                                flex_dma_async_2d(
                                    info->Z_tile_base + info->store_m * info->Z_tile_M_iter_offset + info->store_n * info->Z_tile_N_iter_offset,
                                    DMA_L1_Z,
                                    info->N_tile * DATA_TYPE_BYTE,
                                    info->N_size * DATA_TYPE_BYTE,
                                    info->N_tile * DATA_TYPE_BYTE,
                                    info->M_tile);
                                flex_dma_async_wait_all();
                                flex_dma_async_1d(DMA_L1_Z, zomem(0), info->L1_Z_size);
                                flex_dma_async_wait_all();
                            }
                            info->store_id = start_id;
                        }
                    }

                    // *** LOCAL TILE COMPUTE ***
                    if (flex_is_first_core())
                    {
                        if (info->M_tile <= 1) {
                            // GEMV path: single row, no M-unrolling
                            spatz_tile_gemv(
                                COMPUTE_L1_Z, COMPUTE_L1_X, COMPUTE_L1_W,
                                info->K_tile, info->N_tile);
                        } else {
                            // GEMM path: 4-row unrolled
                            spatz_tile_gemm(
                                COMPUTE_L1_Z, COMPUTE_L1_X, COMPUTE_L1_W,
                                info->M_tile, info->K_tile, info->N_tile);
                        }
                    }
                }

                // End of K-iteration: sync and swap Z buffers
                if (info->group_reduction == 1)
                {
                    flex_global_barrier_xy();
                } else {
                    grid_sync_group_barrier_xy(&(info->group));
                }
                info->store_recorded = 1;
                info->store_m = m;
                info->store_n = n;
                info->store_id = info->summa_group_x;
                uint32_t tmp_z = DMA_L1_Z;
                DMA_L1_Z = COMPUTE_L1_Z;
                COMPUTE_L1_Z = tmp_z;
                // No redmule_wait needed
                flex_intra_cluster_sync();
            }
        }

        // Final store
        if (flex_is_dm_core() && info->store_active == 1)
        {
            if (info->group_reduction == 1)
            {
                flex_dma_async_reduction(
                    DMA_L1_Z, DMA_L1_Z, info->L1_Z_size,
                    COLLECTIVE_REDSUM_TYPE,
                    ~info->group.wakeup_row_mask,
                    ~info->group.wakeup_col_mask);
                flex_dma_async_wait_all();
            }
            flex_dma_async_2d(
                info->Z_tile_base + info->store_m * info->Z_tile_M_iter_offset + info->store_n * info->Z_tile_N_iter_offset,
                DMA_L1_Z,
                info->N_tile * DATA_TYPE_BYTE,
                info->N_size * DATA_TYPE_BYTE,
                info->N_tile * DATA_TYPE_BYTE,
                info->M_tile);
            flex_dma_async_wait_all();
        }
    }

    flex_global_barrier_xy();
}

#endif
