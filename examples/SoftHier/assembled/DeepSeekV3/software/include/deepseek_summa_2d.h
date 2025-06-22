#ifndef _DEEPSEEK_SUMMA_2D_H_
#define _DEEPSEEK_SUMMA_2D_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"

typedef struct DEEPSEEKSUMMA2DInfo
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
    uint32_t                    elem_size;
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

    //Addressing Information
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

    //L1 location information
    uint32_t                    L1_X1;
    uint32_t                    L1_W1;
    uint32_t                    L1_Z1;
    uint32_t                    L1_X2;
    uint32_t                    L1_W2;
    uint32_t                    L1_Z2;

    //Usefull parameters for address calculation
    uint32_t                    L1_X_size;
    uint32_t                    L1_W_size;
    uint32_t                    L1_Z_size;

}DEEPSEEKSUMMA2DInfo;

void DeepSeek_SUMMA_2D(
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
    uint32_t                    X_address_group_gap,
    uint32_t                    W_address_group_gap,
    uint32_t                    Z_address_group_gap){

    flex_global_barrier_xy();

    DEEPSEEKSUMMA2DInfo info;

    info.X_address              = X_address;
    info.W_address              = W_address;
    info.Z_address              = Z_address;
    info.M_size                 = M_size;
    info.N_size                 = N_size;
    info.K_size                 = K_size;
    info.M_tile                 = M_tile;
    info.N_tile                 = N_tile;
    info.K_tile                 = K_tile;
    info.elem_size              = DATA_TYPE_BYTE;
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
    info.K_iter                 = (K_size + K_tile - 1) / K_tile;
    info.store_recorded         = 0;
    info.store_m                = 0;
    info.store_n                = 0;
    info.store_step             = (info.summa_group_x + info.K_iter - 1) / info.K_iter;
    info.store_id               = info.summa_group_x;
    info.X_tile_base            = X_address + X_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_y * M_tile * K_size * info.elem_size; 
    info.W_tile_base            = W_address + W_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_x * N_tile          * info.elem_size;
    info.Z_tile_base            = Z_address + Z_address_group_gap * info.group.this_grid_id + info.cluster_in_group_id_y * M_tile * N_size * info.elem_size + info.cluster_in_group_id_x * N_tile * info.elem_size;
    info.X_tile_M_iter_offset   = info.summa_group_x * M_tile * K_size * info.elem_size;
    info.X_tile_N_iter_offset   = 0;
    info.X_tile_K_iter_offset   = K_tile * info.elem_size;
    info.W_tile_M_iter_offset   = 0;
    info.W_tile_N_iter_offset   = info.summa_group_x * N_tile * info.elem_size;
    info.W_tile_K_iter_offset   = K_tile * N_size * info.elem_size;
    info.Z_tile_M_iter_offset   = info.summa_group_y * M_tile * N_size * info.elem_size;
    info.Z_tile_N_iter_offset   = info.summa_group_x * N_tile * info.elem_size;
    info.L1_X1                  = local(0);
    info.L1_W1                  = info.L1_X1 + M_tile * K_tile * info.elem_size;
    info.L1_Z1                  = info.L1_W1 + N_tile * K_tile * info.elem_size;
    info.L1_X2                  = info.L1_Z1 + M_tile * N_tile * info.elem_size;
    info.L1_W2                  = info.L1_X2 + M_tile * K_tile * info.elem_size;
    info.L1_Z2                  = info.L1_W2 + N_tile * K_tile * info.elem_size;
    info.L1_X_size              = M_tile * K_tile * info.elem_size;
    info.L1_W_size              = N_tile * K_tile * info.elem_size;
    info.L1_Z_size              = M_tile * N_tile * info.elem_size;

    flex_global_barrier_xy();

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    ////////////////////////////////
    //          SUMMA 2D          //
    ////////////////////////////////
    if (info.cluster_active)
    {
        for (int m = 0; m < info.M_iter; ++m)
        {
            for (int n = 0; n < info.N_iter; ++n)
            {
                //Prefetching
                if (flex_is_dm_core())
                {
                    if (info.cluster_in_group_id_x == 0)
                    {
                        //load X from west edge
                        flex_dma_async_2d(
                            info.L1_X1, /*destination*/
                            info.X_tile_base + m * info.X_tile_M_iter_offset + n * info.X_tile_N_iter_offset, /*source*/
                            info.K_tile * info.elem_size, /*transfer size*/
                            info.K_tile * info.elem_size, /*destination stride*/
                            info.K_size * info.elem_size, /*source stride*/
                            info.M_tile /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        if (info.summa_group_x > 1)
                        {
                            flex_dma_async_broadcast(
                                info.L1_X1/*dst_offset*/,
                                info.L1_X1/*src_offset*/,
                                info.L1_X_size/*transfer_size*/,
                                info.group.wakeup_row_mask/*row_mask*/,
                                (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                            flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        }
                    }
                    if (info.cluster_in_group_id_y == 0)
                    {
                        //load W from south edge
                        flex_dma_async_2d(
                            info.L1_W1, /*destination*/
                            info.W_tile_base + m * info.W_tile_M_iter_offset + n * info.W_tile_N_iter_offset, /*source*/
                            info.N_tile * info.elem_size, /*transfer size*/
                            info.N_tile * info.elem_size, /*destination stride*/
                            info.N_size * info.elem_size, /*source stride*/
                            info.K_tile /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        if (info.summa_group_y > 1)
                        {
                            flex_dma_async_broadcast(
                                info.L1_W1/*dst_offset*/,
                                info.L1_W1/*src_offset*/,
                                info.L1_W_size/*transfer_size*/,
                                (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                                info.group.wakeup_col_mask/*col_mask*/);
                            flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        }
                    }
                }

                //Double buffering
                for (int k = 1; k <= info.K_iter; ++k)
                {
                    uint32_t DMA_L1_X;
                    uint32_t DMA_L1_W;
                    uint32_t REDMULE_L1_X;
                    uint32_t REDMULE_L1_W;

                    DMA_L1_X     = (k % 2 == 1)? info.L1_X2 : info.L1_X1;
                    DMA_L1_W     = (k % 2 == 1)? info.L1_W2 : info.L1_W1; 
                    REDMULE_L1_X = (k % 2 == 1)? info.L1_X1 : info.L1_X2;
                    REDMULE_L1_W = (k % 2 == 1)? info.L1_W1 : info.L1_W2;

                    grid_sync_group_barrier_xy(&(info.group));
                    if (flex_is_first_core())
                    {
                        flex_redmule_wait();
                    }
                    flex_intra_cluster_sync();

                    if (flex_is_dm_core())
                    {
                        if (k < info.K_iter)
                        {
                            if (info.cluster_in_group_id_x == 0)
                            {
                                //load X from west edge
                                flex_dma_async_2d(
                                    DMA_L1_X, /*destination*/
                                    info.X_tile_base + m * info.X_tile_M_iter_offset + n * info.X_tile_N_iter_offset + k * info.X_tile_K_iter_offset, /*source*/
                                    info.K_tile * info.elem_size, /*transfer size*/
                                    info.K_tile * info.elem_size, /*destination stride*/
                                    info.K_size * info.elem_size, /*source stride*/
                                    info.M_tile /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //row-wise multicast
                                if (info.summa_group_x > 1)
                                {
                                    flex_dma_async_broadcast(
                                        DMA_L1_X/*dst_offset*/,
                                        DMA_L1_X/*src_offset*/,
                                        info.L1_X_size/*transfer_size*/,
                                        info.group.wakeup_row_mask/*row_mask*/,
                                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                }
                            }
                            if (info.cluster_in_group_id_y == 0)
                            {
                                //load W from south edge
                                flex_dma_async_2d(
                                    DMA_L1_W, /*destination*/
                                    info.W_tile_base + m * info.W_tile_M_iter_offset + n * info.W_tile_N_iter_offset + k * info.W_tile_K_iter_offset, /*source*/
                                    info.N_tile * info.elem_size, /*transfer size*/
                                    info.N_tile * info.elem_size, /*destination stride*/
                                    info.N_size * info.elem_size, /*source stride*/
                                    info.K_tile /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //col-wise multicast
                                if (info.summa_group_y > 1)
                                {
                                    flex_dma_async_broadcast(
                                        DMA_L1_W/*dst_offset*/,
                                        DMA_L1_W/*src_offset*/,
                                        info.L1_W_size/*transfer_size*/,
                                        (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                                        info.group.wakeup_col_mask/*col_mask*/);
                                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                }
                            }
                        }

                        if (info.store_recorded == 1)
                        {
                            uint32_t start_id = (info.store_id < info.store_step)? 0 : info.store_id - info.store_step;
                            if (info.cluster_in_group_id_x >= start_id && info.cluster_in_group_id_x < info.store_id)
                            {
                                //Store Z
                                flex_dma_async_2d(
                                    info.Z_tile_base + info.store_m * info.Z_tile_M_iter_offset + info.store_n * info.Z_tile_N_iter_offset, /*destination*/
                                    info.L1_Z2, /*source*/
                                    info.N_tile * info.elem_size, /*transfer size*/
                                    info.N_size * info.elem_size, /*destination stride*/
                                    info.N_tile * info.elem_size, /*source stride*/
                                    info.M_tile /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                            }
                            info.store_id = start_id;
                        }
                    }

                    if (flex_is_first_core())
                    {
                        flex_redmule_config(info.M_tile, info.K_tile, info.N_tile);
                        flex_redmule_trigger(REDMULE_L1_X, REDMULE_L1_W, info.L1_Z1, REDMULE_COMPUTE_TYPE);
                    }
                }

                //Storing
                grid_sync_group_barrier_xy(&(info.group));
                info.store_recorded = 1;
                info.store_m = m;
                info.store_n = n;
                info.store_id = info.summa_group_x;
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
            }
        }

        if (flex_is_dm_core())
        {
            //Store Z
            flex_dma_async_2d(
                info.Z_tile_base + info.store_m * info.Z_tile_M_iter_offset + info.store_n * info.Z_tile_N_iter_offset, /*destination*/
                info.L1_Z2, /*source*/
                info.N_tile * info.elem_size, /*transfer size*/
                info.N_size * info.elem_size, /*destination stride*/
                info.N_tile * info.elem_size, /*source stride*/
                info.M_tile /*repeat*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
    }

    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

}

#endif