#ifndef _SUMMA_GEMM_H_
#define _SUMMA_GEMM_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"

typedef struct SummaGEMMInfo
{
    //General information
    uint64_t                    X_address;
    uint64_t                    W_address;
    uint64_t                    Z_address;
    uint32_t                    M_size;
    uint32_t                    N_size;
    uint32_t                    K_size; /*shared dimension*/
    uint32_t                    M_tile;
    uint32_t                    N_tile;
    uint32_t                    K_tile;
    uint32_t                    group_reduction;
    uint32_t                    group_splitK;
    uint32_t                    summa_group_x;
    uint32_t                    summa_group_y;
    uint32_t                    summa_groups;

    //Cluster information
    FlexPosition                cluster_pos;
    uint32_t                    cluster_global_id;
    uint32_t                    cluster_active;

    //Group Info
    GridSyncGroupInfo           group;
    uint32_t                    cluster_in_group_id;
    uint32_t                    cluster_in_group_id_x;
    uint32_t                    cluster_in_group_id_y;

    //Tiling information
    uint32_t                    M_iter;
    uint32_t                    N_iter;
    uint32_t                    K_iter;

    //Store Control
    uint32_t                    store_recorded;
    uint32_t                    store_m;
    uint32_t                    store_n;
    uint32_t                    store_step;
    uint32_t                    store_id;
    uint32_t                    store_active;

    //Addressing Information
    uint64_t                    X_tile_base;
    uint64_t                    W_tile_base;
    uint64_t                    Z_tile_base;
#if GEMM_RESHA_X_FROM_ENABLE == 1
    uint64_t                    X_tile_base_offset;
#endif
#if GEMM_RESHA_Z_TO_ENABLE == 1
    uint64_t                    Z_tile_base_offset;
#endif
    uint64_t                    X_tile_M_iter_offset;
    uint64_t                    X_tile_N_iter_offset;
    uint64_t                    X_tile_K_iter_offset;
    uint64_t                    W_tile_M_iter_offset;
    uint64_t                    W_tile_N_iter_offset;
    uint64_t                    W_tile_K_iter_offset;
    uint64_t                    Z_tile_M_iter_offset;
    uint64_t                    Z_tile_N_iter_offset;

    //L1 location information
    uint32_t                    L1_X1;
    uint32_t                    L1_W1;
    uint32_t                    L1_Z1;
    uint32_t                    L1_X2;
    uint32_t                    L1_W2;
    uint32_t                    L1_Z2;
    uint32_t                    L1_AREA;

    //Usefull parameters for address calculation
    uint32_t                    L1_X_size;
    uint32_t                    L1_W_size;
    uint32_t                    L1_Z_size;

}SummaGEMMInfo;

SummaGEMMInfo SummaGEMMAnaylze(
    uint64_t                    X_address,
    uint64_t                    W_address,
    uint64_t                    Z_address,
    uint32_t                    M_size,
    uint32_t                    N_size,
    uint32_t                    K_size, /*shared dimension*/
    uint32_t                    M_tile,
    uint32_t                    N_tile,
    uint32_t                    K_tile,
    uint32_t                    group_x,
    uint32_t                    group_y,
    uint32_t                    num_group,
    uint32_t                    group_reduction,
    uint32_t                    group_splitK,
    uint32_t                    X_address_group_gap,
    uint32_t                    W_address_group_gap,
    uint32_t                    Z_address_group_gap)
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
    info.summa_group_x          = group_x;
    info.summa_group_y          = group_y;
    info.summa_groups           = num_group;


    //Group infomation
    FlexPosition pos            = get_pos(flex_get_cluster_id());
    info.cluster_pos            = pos;
    info.cluster_global_id      = flex_get_cluster_id();
    info.group                  = grid_sync_group_init(info.summa_group_x,info.summa_group_y);
    info.cluster_in_group_id_x  = pos.x % info.group.grid_x_dim;
    info.cluster_in_group_id_y  = pos.y % info.group.grid_y_dim;
    info.cluster_in_group_id    = info.cluster_in_group_id_x + info.group.this_grid_cluster_num_x * info.cluster_in_group_id_y;
    info.cluster_active         = info.group.this_grid_id < info.summa_groups ? 1 : 0;

    info.M_iter                 = (M_size + info.summa_group_y * M_tile - 1) / (info.summa_group_y * M_tile);
    info.N_iter                 = (N_size + info.summa_group_x * N_tile - 1) / (info.summa_group_x * N_tile);
    info.K_iter                 = (info.group_reduction && info.group_splitK)? (((K_size / info.summa_groups) + K_tile - 1) / K_tile) : ((K_size + K_tile - 1) / K_tile);
    info.store_recorded         = 0;
    info.store_m                = 0;
    info.store_n                = 0;
    info.store_step             = (info.summa_group_x + info.K_iter - 1) / info.K_iter;
    info.store_id               = info.summa_group_x;
    info.store_active           = (info.group_reduction == 0) ? 1 : (info.group.this_grid_id == 0)? 1 : 0;
#if GEMM_RESHA_X_FROM_ENABLE == 1
    info.X_tile_base_offset     = info.cluster_in_group_id_y * M_tile * K_size * DATA_TYPE_BYTE;
    info.X_tile_base            = X_address + X_address_group_gap * info.group.this_grid_id;
#else
    info.X_tile_base            = X_address + X_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_y * M_tile * K_size * DATA_TYPE_BYTE; 
#endif
    info.W_tile_base            = W_address + W_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_x * N_tile          * DATA_TYPE_BYTE;
#if GEMM_RESHA_Z_TO_ENABLE == 1
    info.Z_tile_base_offset     = info.cluster_in_group_id_y * M_tile * N_size * DATA_TYPE_BYTE + info.cluster_in_group_id_x * N_tile * DATA_TYPE_BYTE;
    info.Z_tile_base            = Z_address + Z_address_group_gap * info.group.this_grid_id;
#else
    info.Z_tile_base            = Z_address + Z_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_y * M_tile * N_size * DATA_TYPE_BYTE + info.cluster_in_group_id_x * N_tile * DATA_TYPE_BYTE;
#endif
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
        uint32_t REDMULE_L1_Z = info->L1_Z1;

        //Initialize Z buffer
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(info->L1_Z1,zomem(0),info->L1_Z_size);
            flex_dma_async_1d(info->L1_Z2,zomem(0),info->L1_Z_size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        for (int m = 0; m < info->M_iter; ++m)
        {
            for (int n = 0; n < info->N_iter; ++n)
            {
                //Prefetching
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load X from west edge
#if GEMM_RESHA_X_FROM_ENABLE == 1
                        uint64_t origin_elem_offest = (info->X_tile_base_offset + m * info->X_tile_M_iter_offset + n * info->X_tile_N_iter_offset) / DATA_TYPE_BYTE;
                        uint64_t origin_k = origin_elem_offest % GEMM_K_SIZE;
                        uint64_t origin_m = origin_elem_offest / GEMM_K_SIZE;
                        #if defined(GEMM_RESHA_X_FROM_TALL)
                            uint64_t num_bulk = origin_k / GEMM_RESHAPE_X_FROM_K;
                            uint64_t mapped_k = origin_k % GEMM_RESHAPE_X_FROM_K;
                            uint64_t mapped_m = num_bulk * info->M_size + origin_m;
                        #endif
                        #if defined(GEMM_RESHA_X_FROM_THIN)
                            uint64_t num_bulk = origin_m / GEMM_RESHA_X_FROM_M;
                            uint64_t mapped_m = origin_m % GEMM_RESHA_X_FROM_M;
                            uint64_t mapped_k = num_bulk * info->K_size + origin_k;
                        #endif
                        uint64_t mapped_offset = (mapped_m * GEMM_RESHAPE_X_FROM_K + mapped_k) * DATA_TYPE_BYTE;
                        flex_dma_async_2d(
                            info->L1_X1, /*destination*/
                            info->X_tile_base + mapped_offset, /*source*/
                            info->K_tile * DATA_TYPE_BYTE, /*transfer size*/
                            info->K_tile * DATA_TYPE_BYTE, /*destination stride*/
                            GEMM_RESHAPE_X_FROM_K * DATA_TYPE_BYTE, /*source stride*/
                            info->M_tile /*repeat*/); //Start 2D iDMA
#else
                        flex_dma_async_2d(
                            info->L1_X1, /*destination*/
                            info->X_tile_base + m * info->X_tile_M_iter_offset + n * info->X_tile_N_iter_offset, /*source*/
                            info->K_tile * DATA_TYPE_BYTE, /*transfer size*/
                            info->K_tile * DATA_TYPE_BYTE, /*destination stride*/
                            info->K_size * DATA_TYPE_BYTE, /*source stride*/
                            info->M_tile /*repeat*/); //Start 2D iDMA
#endif
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        if (info->summa_group_x > 1)
                        {
                            flex_dma_async_broadcast(
                                info->L1_X1/*dst_offset*/,
                                info->L1_X1/*src_offset*/,
                                info->L1_X_size/*transfer_size*/,
                                info->group.wakeup_row_mask/*row_mask*/,
                                (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                            flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        }
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load W from south edge
                        flex_dma_async_2d(
                            info->L1_W1, /*destination*/
                            info->W_tile_base + m * info->W_tile_M_iter_offset + n * info->W_tile_N_iter_offset, /*source*/
                            info->N_tile * DATA_TYPE_BYTE, /*transfer size*/
                            info->N_tile * DATA_TYPE_BYTE, /*destination stride*/
                            info->N_size * DATA_TYPE_BYTE, /*source stride*/
                            info->K_tile /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        if (info->summa_group_y > 1)
                        {
                            flex_dma_async_broadcast(
                                info->L1_W1/*dst_offset*/,
                                info->L1_W1/*src_offset*/,
                                info->L1_W_size/*transfer_size*/,
                                (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                                info->group.wakeup_col_mask/*col_mask*/);
                            flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        }
                    }
                }

                //Double buffering
                for (int k = 1; k <= info->K_iter; ++k)
                {
                    uint32_t DMA_L1_X;
                    uint32_t DMA_L1_W;
                    uint32_t REDMULE_L1_X;
                    uint32_t REDMULE_L1_W;

                    DMA_L1_X     = (k % 2 == 1)? info->L1_X2 : info->L1_X1;
                    DMA_L1_W     = (k % 2 == 1)? info->L1_W2 : info->L1_W1; 
                    REDMULE_L1_X = (k % 2 == 1)? info->L1_X1 : info->L1_X2;
                    REDMULE_L1_W = (k % 2 == 1)? info->L1_W1 : info->L1_W2;

                    grid_sync_group_barrier_xy(&(info->group));
                    if (flex_is_first_core())
                    {
                        flex_redmule_wait();
                    }
                    flex_intra_cluster_sync();

                    if (flex_is_dm_core())
                    {
                        if (k < info->K_iter)
                        {
                            if (info->cluster_in_group_id_x == 0)
                            {
                                //load X from west edge
#if GEMM_RESHA_X_FROM_ENABLE == 1
                                uint64_t origin_elem_offest = (info->X_tile_base_offset + m * info->X_tile_M_iter_offset + n * info->X_tile_N_iter_offset + k * info->X_tile_K_iter_offset) / DATA_TYPE_BYTE;
                                uint64_t origin_k = origin_elem_offest % GEMM_K_SIZE;
                                uint64_t origin_m = origin_elem_offest / GEMM_K_SIZE;
                                #if defined(GEMM_RESHA_X_FROM_TALL)
                                    uint64_t num_bulk = origin_k / GEMM_RESHAPE_X_FROM_K;
                                    uint64_t mapped_k = origin_k % GEMM_RESHAPE_X_FROM_K;
                                    uint64_t mapped_m = num_bulk * info->M_size + origin_m;
                                #endif
                                #if defined(GEMM_RESHA_X_FROM_THIN)
                                    uint64_t num_bulk = origin_m / GEMM_RESHA_X_FROM_M;
                                    uint64_t mapped_m = origin_m % GEMM_RESHA_X_FROM_M;
                                    uint64_t mapped_k = num_bulk * info->K_size + origin_k;
                                #endif
                                uint64_t mapped_offset = (mapped_m * GEMM_RESHAPE_X_FROM_K + mapped_k) * DATA_TYPE_BYTE;
                                flex_dma_async_2d(
                                    DMA_L1_X, /*destination*/
                                    info->X_tile_base + mapped_offset, /*source*/
                                    info->K_tile * DATA_TYPE_BYTE, /*transfer size*/
                                    info->K_tile * DATA_TYPE_BYTE, /*destination stride*/
                                    GEMM_RESHAPE_X_FROM_K * DATA_TYPE_BYTE, /*source stride*/
                                    info->M_tile /*repeat*/); //Start 2D iDMA
#else
                                flex_dma_async_2d(
                                    DMA_L1_X, /*destination*/
                                    info->X_tile_base + m * info->X_tile_M_iter_offset + n * info->X_tile_N_iter_offset + k * info->X_tile_K_iter_offset, /*source*/
                                    info->K_tile * DATA_TYPE_BYTE, /*transfer size*/
                                    info->K_tile * DATA_TYPE_BYTE, /*destination stride*/
                                    info->K_size * DATA_TYPE_BYTE, /*source stride*/
                                    info->M_tile /*repeat*/); //Start 2D iDMA
#endif
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //row-wise multicast
                                if (info->summa_group_x > 1)
                                {
                                    flex_dma_async_broadcast(
                                        DMA_L1_X/*dst_offset*/,
                                        DMA_L1_X/*src_offset*/,
                                        info->L1_X_size/*transfer_size*/,
                                        info->group.wakeup_row_mask/*row_mask*/,
                                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                }
                            }
                            if (info->cluster_in_group_id_y == 0)
                            {
                                //load W from south edge
                                flex_dma_async_2d(
                                    DMA_L1_W, /*destination*/
                                    info->W_tile_base + m * info->W_tile_M_iter_offset + n * info->W_tile_N_iter_offset + k * info->W_tile_K_iter_offset, /*source*/
                                    info->N_tile * DATA_TYPE_BYTE, /*transfer size*/
                                    info->N_tile * DATA_TYPE_BYTE, /*destination stride*/
                                    info->N_size * DATA_TYPE_BYTE, /*source stride*/
                                    info->K_tile /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //col-wise multicast
                                if (info->summa_group_y > 1)
                                {
                                    flex_dma_async_broadcast(
                                        DMA_L1_W/*dst_offset*/,
                                        DMA_L1_W/*src_offset*/,
                                        info->L1_W_size/*transfer_size*/,
                                        (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                                        info->group.wakeup_col_mask/*col_mask*/);
                                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                }
                            }
                        }

                        if (info->store_recorded == 1 && info->store_active == 1)
                        {
                            uint32_t start_id = (info->store_id < info->store_step)? 0 : info->store_id - info->store_step;
                            if (info->cluster_in_group_id_x >= start_id && info->cluster_in_group_id_x < info->store_id)
                            {
                                if (info->group_reduction == 1)
                                {
                                    //Reduce Z
                                    flex_dma_async_reduction(
                                        DMA_L1_Z/*dst_offset*/,
                                        DMA_L1_Z/*src_offset*/,
                                        info->L1_Z_size/*transfer_size*/,
                                        COLLECTIVE_REDSUM_TYPE/*fmt*/,
                                        ~info->group.wakeup_row_mask/*row_mask*/,
                                        ~info->group.wakeup_col_mask/*col_mask*/);
                                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                }
                                //Store Z
#if GEMM_RESHA_Z_TO_ENABLE == 1
                                uint64_t origin_elem_offest = (info->Z_tile_base_offset + info->store_m * info->Z_tile_M_iter_offset + info->store_n * info->Z_tile_N_iter_offset) / DATA_TYPE_BYTE;
                                uint64_t origin_n = origin_elem_offest % GEMM_N_SIZE;
                                uint64_t origin_m = origin_elem_offest / GEMM_N_SIZE;
                                #if defined(GEMM_RESHA_Z_TO_TALL)
                                    uint64_t num_buln = origin_n / GEMM_RESHAPE_Z_TO_N;
                                    uint64_t mapped_n = origin_n % GEMM_RESHAPE_Z_TO_N;
                                    uint64_t mapped_m = num_buln * info->M_size + origin_m;
                                #endif
                                #if defined(GEMM_RESHA_Z_TO_THIN)
                                    uint64_t num_buln = origin_m / GEMM_RESHA_Z_TO_M;
                                    uint64_t mapped_m = origin_m % GEMM_RESHA_Z_TO_M;
                                    uint64_t mapped_n = num_buln * info->N_size + origin_n;
                                #endif
                                uint64_t mapped_offset = (mapped_m * GEMM_RESHAPE_Z_TO_N + mapped_n) * DATA_TYPE_BYTE;
                                flex_dma_async_2d(
                                    info->Z_tile_base + mapped_offset, /*destination*/
                                    DMA_L1_Z, /*source*/
                                    info->N_tile * DATA_TYPE_BYTE, /*transfer size*/
                                    GEMM_RESHAPE_Z_TO_N * DATA_TYPE_BYTE, /*destination stride*/
                                    info->N_tile * DATA_TYPE_BYTE, /*source stride*/
                                    info->M_tile /*repeat*/); //Start 2D iDMA
#else
                                flex_dma_async_2d(
                                    info->Z_tile_base + info->store_m * info->Z_tile_M_iter_offset + info->store_n * info->Z_tile_N_iter_offset, /*destination*/
                                    DMA_L1_Z, /*source*/
                                    info->N_tile * DATA_TYPE_BYTE, /*transfer size*/
                                    info->N_size * DATA_TYPE_BYTE, /*destination stride*/
                                    info->N_tile * DATA_TYPE_BYTE, /*source stride*/
                                    info->M_tile /*repeat*/); //Start 2D iDMA
#endif
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //Clear Z
                                flex_dma_async_1d(DMA_L1_Z,zomem(0),info->L1_Z_size);
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                            }
                            info->store_id = start_id;
                        }
                    }

                    if (flex_is_first_core())
                    {
                        flex_redmule_config(info->M_tile, info->K_tile, info->N_tile);
                        flex_redmule_trigger(REDMULE_L1_X, REDMULE_L1_W, REDMULE_L1_Z, REDMULE_COMPUTE_TYPE);
                    }
                }

                //Storing
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
                DMA_L1_Z = REDMULE_L1_Z;
                REDMULE_L1_Z = tmp_z;
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
            }
        }

        if (flex_is_dm_core() && info->store_active == 1)
        {
            if (info->group_reduction == 1)
            {
                //Reduce Z
                flex_dma_async_reduction(
                    DMA_L1_Z/*dst_offset*/,
                    DMA_L1_Z/*src_offset*/,
                    info->L1_Z_size/*transfer_size*/,
                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                    ~info->group.wakeup_row_mask/*row_mask*/,
                    ~info->group.wakeup_col_mask/*col_mask*/);
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
            }
            //Store Z
#if GEMM_RESHA_Z_TO_ENABLE == 1
            uint64_t origin_elem_offest = (info->Z_tile_base_offset + info->store_m * info->Z_tile_M_iter_offset + info->store_n * info->Z_tile_N_iter_offset) / DATA_TYPE_BYTE;
            uint64_t origin_n = origin_elem_offest % GEMM_N_SIZE;
            uint64_t origin_m = origin_elem_offest / GEMM_N_SIZE;
            #if defined(GEMM_RESHA_Z_TO_TALL)
                uint64_t num_buln = origin_n / GEMM_RESHAPE_Z_TO_N;
                uint64_t mapped_n = origin_n % GEMM_RESHAPE_Z_TO_N;
                uint64_t mapped_m = num_buln * info->M_size + origin_m;
            #endif
            #if defined(GEMM_RESHA_Z_TO_THIN)
                uint64_t num_buln = origin_m / GEMM_RESHA_Z_TO_M;
                uint64_t mapped_m = origin_m % GEMM_RESHA_Z_TO_M;
                uint64_t mapped_n = num_buln * info->N_size + origin_n;
            #endif
            uint64_t mapped_offset = (mapped_m * GEMM_RESHAPE_Z_TO_N + mapped_n) * DATA_TYPE_BYTE;
            flex_dma_async_2d(
                info->Z_tile_base + mapped_offset, /*destination*/
                DMA_L1_Z, /*source*/
                info->N_tile * DATA_TYPE_BYTE, /*transfer size*/
                GEMM_RESHAPE_Z_TO_N * DATA_TYPE_BYTE, /*destination stride*/
                info->N_tile * DATA_TYPE_BYTE, /*source stride*/
                info->M_tile /*repeat*/); //Start 2D iDMA
#else
            flex_dma_async_2d(
                info->Z_tile_base + info->store_m * info->Z_tile_M_iter_offset + info->store_n * info->Z_tile_N_iter_offset, /*destination*/
                DMA_L1_Z, /*source*/
                info->N_tile * DATA_TYPE_BYTE, /*transfer size*/
                info->N_size * DATA_TYPE_BYTE, /*destination stride*/
                info->N_tile * DATA_TYPE_BYTE, /*source stride*/
                info->M_tile /*repeat*/); //Start 2D iDMA
#endif
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
    }

    flex_global_barrier_xy();
}


#endif