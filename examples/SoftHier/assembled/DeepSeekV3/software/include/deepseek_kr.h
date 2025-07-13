#ifndef _DEEPSEEK_KR_H_
#define _DEEPSEEK_KR_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"

void DeepSeek_KR(uint32_t seq_len)
{
    //Analyze
    uint32_t                    num_group   = (seq_len + 127) / 128;
    uint32_t                    M           = seq_len > 128? 128 : seq_len;
    FlexPosition                pos         = get_pos(flex_get_cluster_id());
    GridSyncGroupInfo           group       = grid_sync_group_init(1,32);
    uint32_t                    active      = group.this_grid_id < num_group ? 1 : 0;
    uint32_t                    L1_I        = local(0);
    uint32_t                    L1_W        = L1_I + M  * 224 * DATA_TYPE_BYTE;
    uint32_t                    L1_O        = L1_W + 64 * 224 * DATA_TYPE_BYTE;

    flex_global_barrier_xy();

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    ///////////////////////////////////////////
    //              DeepSeek KR              //
    ///////////////////////////////////////////

    //1. col-wise load Weight and row-wise multicast
    if (pos.x == pos.y && flex_is_dm_core())
    {
        flex_dma_async_1d(
            L1_W, /*destination*/
            hbm_south(0,0), /*source*/
            64 * 224 * DATA_TYPE_BYTE/*transfer size*/); //Start 1D iDMA
        flex_dma_async_wait_all(); // Wait for iDMA Finishing
        flex_dma_async_broadcast(
            L1_W/*dst_offset*/,
            L1_W/*src_offset*/,
            64 * 224 * DATA_TYPE_BYTE/*transfer_size*/,
            0/*row_mask*/,
            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
        flex_dma_async_wait_all(); // Wait for iDMA Finishing
    }
    flex_global_barrier_xy();

    if (active)
    {
        //2. load input
        if (flex_is_dm_core())
        {
            flex_dma_async_2d(
                L1_I, /*destination*/
                hbm_west(0,0) + pos.x * M * 7168 * DATA_TYPE_BYTE + pos.y * 224 * DATA_TYPE_BYTE, /*source*/
                224  * DATA_TYPE_BYTE, /*transfer size*/
                224  * DATA_TYPE_BYTE, /*destination stride*/
                7168 * DATA_TYPE_BYTE, /*source stride*/
                M /*repeat*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
        flex_intra_cluster_sync();

        //3. compute
        if (flex_is_first_core())
        {
            flex_redmule_config(M, 224, 64);
            flex_redmule_trigger(L1_I, L1_W, L1_O, REDMULE_COMPUTE_TYPE);
            flex_redmule_wait();
        }
        grid_sync_group_barrier_xy(&group);

        if (pos.x == pos.y && flex_is_dm_core())
        {
            //4. col-wise reduction
            flex_dma_async_reduction(
                L1_O/*dst_offset*/,
                L1_O/*src_offset*/,
                M * 64 * DATA_TYPE_BYTE/*transfer_size*/,
                COLLECTIVE_REDSUM_TYPE/*fmt*/,
                (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                0/*col_mask*/);
            flex_dma_async_wait_all(); // Wait for iDMA Finishing

            //5. store back
            flex_dma_async_1d(
                hbm_west(0,0) + pos.x * M * 64 * DATA_TYPE_BYTE, /*destination*/
                L1_O, /*source*/
                M * 64 * DATA_TYPE_BYTE/*transfer size*/); //Start 1D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
    }

    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();
}

#endif