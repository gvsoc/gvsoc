#ifndef _SOFTWARE_COLLECTIBES
#define _SOFTWARE_COLLECTIBES

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_transpose_engine.h"
#include "flex_libfp16.h"
#include "flex_libfp8.h"
#include "flex_dump.h"

typedef struct CollectiveCompare
{
    //Group infomation
    GridSyncGroupInfo       group;

    //Cluster information
    FlexPosition            cluster_pos;
    uint32_t                cluster_global_id;
    uint32_t                cluster_in_group_id;
    uint32_t                cluster_in_group_id_x;
    uint32_t                cluster_in_group_id_y;
    uint32_t                cluster_for_rowwise;
    uint32_t                cluster_for_colwise;
}CollectiveCompare;


CollectiveCompare analyze(){

    CollectiveCompare info;

    //initialize group
    info.group                  = grid_sync_group_init(32,32);

    //Cluster information
    FlexPosition pos            = get_pos(flex_get_cluster_id());
    info.cluster_global_id      = flex_get_cluster_id();
    info.cluster_pos            = pos;
    info.cluster_in_group_id_x  = pos.x % info.group.grid_x_dim;
    info.cluster_in_group_id_y  = pos.y % info.group.grid_y_dim;
    info.cluster_in_group_id    = info.cluster_in_group_id_x + info.group.this_grid_cluster_num_x * info.cluster_in_group_id_y;
    info.cluster_for_rowwise    = ((info.cluster_in_group_id_x % info.group.grid_y_dim) == (info.cluster_in_group_id_y % info.group.grid_x_dim) && (info.cluster_in_group_id_x == (pos.y % info.group.grid_x_dim)))? 1 : 0;
    info.cluster_for_colwise    = ((info.cluster_in_group_id_x % info.group.grid_y_dim) == (info.cluster_in_group_id_y % info.group.grid_x_dim) && (info.cluster_in_group_id_y == (pos.x % info.group.grid_y_dim)))? 1 : 0;

    return info;
}

inline void vector_lib_max(
    uint32_t i_addr,
    uint32_t o_addr,
    uint32_t vlen)
{
    uint32_t avl;
    while(vlen > 0){
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(avl) : "r"(vlen));
        asm volatile("vle8.v v8,  (%0)" ::"r"(i_addr));
        asm volatile("vle8.v v0,  (%0)" ::"r"(o_addr));
        asm volatile("vfmax.vv v8, v8, v0");
        asm volatile("vse8.v v8,  (%0)" ::"r"(o_addr));
        vlen -= avl;
        i_addr += avl;
        o_addr += avl;
    }
}

inline void vector_lib_sum(
    uint32_t i_addr,
    uint32_t o_addr,
    uint32_t vlen)
{
    uint32_t avl;
    while(vlen > 0){
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(avl) : "r"(vlen));
        asm volatile("vle8.v v8,  (%0)" ::"r"(i_addr));
        asm volatile("vle8.v v0,  (%0)" ::"r"(o_addr));
        asm volatile("vfadd.vv v8, v8, v0");
        asm volatile("vse8.v v8,  (%0)" ::"r"(o_addr));
        vlen -= avl;
        i_addr += avl;
        o_addr += avl;
    }
}

// Pipelined Collectives
inline void swcoll_pipeline_broadcast_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size, uint32_t num_stages){
    // Calculate the block size of each stage
    uint32_t stage_size = (size + num_stages - 1) / num_stages;
    uint32_t real_stages = (size + stage_size - 1) / stage_size;

    // Pipeline control
    uint32_t is_last_cluster = (info->cluster_in_group_id_x + 1) == info->group.grid_x_dim;
    uint32_t start_step = info->cluster_in_group_id_x;

    // Execute Pipelined Broadcast
    for (int i = 0; i < (real_stages + info->group.grid_x_dim - 1); ++i)
    {
        if (flex_is_dm_core() && (is_last_cluster == 0) && (i >= start_step) && (i < (start_step + real_stages)))
        {
            uint32_t stage_offset = offset + (i - start_step) * stage_size;
            flex_dma_async_1d(
                remote_pos(right_pos(info->cluster_pos),stage_offset),
                local(stage_offset),
                stage_size);
            flex_dma_async_wait_all();
        }
        flex_global_barrier_xy();
    }
}

inline void swcoll_pipeline_broadcast_colwise(CollectiveCompare* info, uint32_t offset, uint32_t size, uint32_t num_stages){
    // Calculate the block size of each stage
    uint32_t stage_size = (size + num_stages - 1) / num_stages;
    uint32_t real_stages = (size + stage_size - 1) / stage_size;

    // Pipeline control
    uint32_t is_last_cluster = (info->cluster_in_group_id_y + 1) == info->group.grid_y_dim;
    uint32_t start_step = info->cluster_in_group_id_y;

    // Execute Pipelined Broadcast
    for (int i = 0; i < (real_stages + info->group.grid_x_dim - 1); ++i)
    {
        if (flex_is_dm_core() && (is_last_cluster == 0) && (i >= start_step) && (i < (start_step + real_stages)))
        {
            uint32_t stage_offset = offset + (i - start_step) * stage_size;
            flex_dma_async_1d(
                remote_pos(bottom_pos(info->cluster_pos),stage_offset),
                local(stage_offset),
                stage_size);
            flex_dma_async_wait_all();
        }
        flex_global_barrier_xy();
    }
}

inline void swcoll_pipeline_redmax_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size, uint32_t num_stages){
    // Calculate the block size of each stage
    uint32_t stage_size = (size + num_stages - 1) / num_stages;
    uint32_t real_stages = (size + stage_size - 1) / stage_size;

    // Pipeline control
    uint32_t is_first_cluster = info->cluster_in_group_id_x == 0;
    uint32_t start_step = info->cluster_in_group_id_x;

    // Execute Pipelined Broadcast
    for (int i = 0; i < (real_stages + info->group.grid_x_dim); ++i)
    {
        if ((is_first_cluster == 0) && (i >= start_step) && (i < (start_step + real_stages)))
        {
            uint32_t stage_offset = offset + (i - start_step) * stage_size;

            //1. load from left
            if (flex_is_dm_core())
            {
                flex_dma_async_1d(
                    local(stage_offset),
                    remote_pos(left_pos(info->cluster_pos),stage_offset),
                    stage_size);
                flex_dma_async_wait_all();
            }
            flex_intra_cluster_sync();

            //2. calculated max
            if (flex_is_first_core())
            {
                vector_lib_max(
                    local(stage_offset)/*i_addr*/,
                    local(stage_offset)/*o_addr*/,
                    stage_size/*vlen*/);
            }
        }
        flex_global_barrier_xy();
    }
}

inline void swcoll_pipeline_redsum_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size, uint32_t num_stages){
    // Calculate the block size of each stage
    uint32_t stage_size = (size + num_stages - 1) / num_stages;
    uint32_t real_stages = (size + stage_size - 1) / stage_size;

    // Pipeline control
    uint32_t is_first_cluster = info->cluster_in_group_id_x == 0;
    uint32_t start_step = info->cluster_in_group_id_x;

    // Execute Pipelined Broadcast
    for (int i = 0; i < (real_stages + info->group.grid_x_dim); ++i)
    {
        if ((is_first_cluster == 0) && (i >= start_step) && (i < (start_step + real_stages)))
        {
            uint32_t stage_offset = offset + (i - start_step) * stage_size;

            //1. load from left
            if (flex_is_dm_core())
            {
                flex_dma_async_1d(
                    local(stage_offset),
                    remote_pos(left_pos(info->cluster_pos),stage_offset),
                    stage_size);
                flex_dma_async_wait_all();
            }
            flex_intra_cluster_sync();

            //2. calculated max
            if (flex_is_first_core())
            {
                vector_lib_sum(
                    local(stage_offset)/*i_addr*/,
                    local(stage_offset)/*o_addr*/,
                    stage_size/*vlen*/);
            }
        }
        flex_global_barrier_xy();
    }
}


// Log Tree
inline void swcoll_logtree_broadcast_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && (info->cluster_pos.x % 32 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x + 16, info->cluster_pos.y, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();

    if (flex_is_dm_core() && (info->cluster_pos.x % 16 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x + 8, info->cluster_pos.y, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();

    if (flex_is_dm_core() && (info->cluster_pos.x % 8 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x + 4, info->cluster_pos.y, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();

    if (flex_is_dm_core() && (info->cluster_pos.x % 4 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x + 2, info->cluster_pos.y, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();

    if (flex_is_dm_core() && (info->cluster_pos.x % 2 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x + 1, info->cluster_pos.y, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();
}

inline void swcoll_logtree_broadcast_colwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && (info->cluster_pos.y % 32 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x, info->cluster_pos.y + 16, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();

    if (flex_is_dm_core() && (info->cluster_pos.y % 16 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x, info->cluster_pos.y + 8, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();

    if (flex_is_dm_core() && (info->cluster_pos.y % 8 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x, info->cluster_pos.y + 4, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();

    if (flex_is_dm_core() && (info->cluster_pos.y % 4 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x, info->cluster_pos.y + 2, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();

    if (flex_is_dm_core() && (info->cluster_pos.y % 2 == 0))
    {
        flex_dma_async_1d(
            remote_xy(info->cluster_pos.x, info->cluster_pos.y + 1, offset),
            local(offset),
            size);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();
}

inline void swcoll_logtree_redmax_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (info->cluster_pos.x % 2 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 1, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_max(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();

    if (info->cluster_pos.x % 4 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 2, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_max(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();

    if (info->cluster_pos.x % 8 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 4, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_max(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();

    if (info->cluster_pos.x % 16 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 8, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_max(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();

    if (info->cluster_pos.x % 32 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 16, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_max(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();
}

inline void swcoll_logtree_redsum_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (info->cluster_pos.x % 2 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 1, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_sum(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();

    if (info->cluster_pos.x % 4 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 2, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_sum(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();

    if (info->cluster_pos.x % 8 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 4, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_sum(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();

    if (info->cluster_pos.x % 16 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 8, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_sum(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();

    if (info->cluster_pos.x % 32 == 0)
    {
        //1. load from left
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                local(offset),
                remote_xy(info->cluster_pos.x + 16, info->cluster_pos.y, offset),
                size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        //2. calculated max
        if (flex_is_first_core())
        {
            vector_lib_sum(
                local(offset)/*i_addr*/,
                local(offset)/*o_addr*/,
                size/*vlen*/);
        }
    }
    flex_global_barrier_xy();
}


// Multi Unicast
inline void swcoll_multiunicast_broadcast_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_pos.x == 0)
    {
        for (int x = 1; x < 32; ++x)
        {
            flex_dma_async_1d(
                remote_xy(x, info->cluster_pos.y, offset),
                local(offset),
                size);
            flex_dma_async_wait_all();
        }
    }
    flex_global_barrier_xy();
}

inline void swcoll_multiunicast_broadcast_colwise(CollectiveCompare* info, uint32_t offset, uint32_t size){

}

inline void swcoll_multiunicast_redmax_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){

}

inline void swcoll_multiunicast_redsum_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (info->cluster_pos.x == 0)
    {
        for (int x = 1; x < 32; ++x)
        {
            if (flex_is_dm_core())
            {
                flex_dma_async_1d(
                    local(offset),
                    remote_xy(x, info->cluster_pos.y, offset),
                    size);
                flex_dma_async_wait_all();
            }
            flex_intra_cluster_sync();

            if (flex_is_first_core())
            {
                vector_lib_sum(
                    local(offset)/*i_addr*/,
                    local(offset)/*o_addr*/,
                    size/*vlen*/);
            }
            flex_intra_cluster_sync();
        }
    }
    flex_global_barrier_xy();
}


inline void flat_attention_broadcast_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_for_rowwise == 1)
    {
        flex_dma_async_broadcast(
            offset/*dst_offset*/,
            offset/*src_offset*/,
            size/*transfer_size*/,
            info->group.wakeup_row_mask/*row_mask*/,
            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
        flex_dma_async_wait_all();
    }
}

inline void flat_attention_broadcast_colwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_for_colwise == 1)
    {
        flex_dma_async_broadcast(
            offset/*dst_offset*/,
            offset/*src_offset*/,
            size/*transfer_size*/,
            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
            info->group.wakeup_col_mask/*col_mask*/);
        flex_dma_async_wait_all();
    }
}

inline void flat_attention_redmax_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_for_rowwise == 1)
    {
        flex_dma_async_reduction(
            offset/*dst_offset*/,
            offset/*src_offset*/,
            size/*transfer_size*/,
            COLLECTIVE_REDMAX_FP_8/*fmt*/,
            info->group.wakeup_row_mask/*row_mask*/,
            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
        flex_dma_async_wait_all();
    }
}

inline void flat_attention_redsum_rowwise(CollectiveCompare* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_for_rowwise == 1)
    {
        flex_dma_async_reduction(
            offset/*dst_offset*/,
            offset/*src_offset*/,
            size/*transfer_size*/,
            COLLECTIVE_REDADD_FP_8/*fmt*/,
            info->group.wakeup_row_mask/*row_mask*/,
            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
        flex_dma_async_wait_all();
    }
}


#endif