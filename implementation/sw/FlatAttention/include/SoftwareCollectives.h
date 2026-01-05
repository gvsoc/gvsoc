#ifndef _SOFTWARE_COLLECTIBES
#define _SOFTWARE_COLLECTIBES

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
        i_addr += DATA_TYPE_BYTE*avl;
        o_addr += DATA_TYPE_BYTE*avl;
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
        i_addr += DATA_TYPE_BYTE*avl;
        o_addr += DATA_TYPE_BYTE*avl;
    }
}

// Pipelined Collectives
inline void swcoll_pipeline_broadcast_rowwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size, uint32_t num_stages){
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
        grid_sync_group_barrier_xy(&(info->group));
    }
}

inline void swcoll_pipeline_broadcast_colwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size, uint32_t num_stages){
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
        grid_sync_group_barrier_xy(&(info->group));
    }
}

inline void swcoll_pipeline_redmax_rowwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size, uint32_t num_stages){
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
        grid_sync_group_barrier_xy(&(info->group));
    }
}

inline void swcoll_pipeline_redsum_rowwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size, uint32_t num_stages){
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
        grid_sync_group_barrier_xy(&(info->group));
    }
}


#endif