#ifndef _DEEPSEEK_MOE_GATE_H_
#define _DEEPSEEK_MOE_GATE_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"

#define DEEPSEEK_MOE_GATE_H 128
#define DEEPSEEK_MOE_GATE_K 56

typedef struct DeepSeekMoEGateInfo
{
    //General information
    uint64_t                input_address;
    uint64_t                weight_address;
    uint64_t                index_address;
    uint64_t                score_address;
    uint32_t                seq_length;

    //Group Info
    GridSyncGroupInfo       group;
    uint32_t                cluster_in_group_id;
    uint32_t                cluster_in_group_id_x;
    uint32_t                cluster_in_group_id_y;

    //Cluster information
    FlexPosition            cluster_pos;
    uint32_t                cluster_global_id;
    uint32_t                cluster_active;

    //Tiling information
    uint32_t                S;
    uint32_t                H;
    uint32_t                E;

    //Usefull parameters for address calculation
    uint32_t                L1_I_size;
    uint32_t                L1_W_size;
    uint32_t                L1_O_size;

    //L1 location information
    uint32_t                L1_I1;
    uint32_t                L1_W1;
    uint32_t                L1_ID;
    uint32_t                L1_VA;
    uint32_t                L1_area;

}DeepSeekMoEGateInfo;

void DeepSeek_MoE_gate_print_info(DeepSeekMoEGateInfo *info) {
    uint32_t H, L;

    // General Information
    printf("    General Information\n");
    H = info->input_address >> 32;
    L = (uint32_t)(info->input_address);
    printf("        input_address:                  0x%08x_%08x\n", H, L);
    H = info->weight_address >> 32;
    L = (uint32_t)(info->weight_address);
    printf("        weight_address:                 0x%08x_%08x\n", H, L);
    H = info->index_address >> 32;
    L = (uint32_t)(info->index_address);
    printf("        index_address:                  0x%08x_%08x\n", H, L);
    H = info->score_address >> 32;
    L = (uint32_t)(info->score_address);
    printf("        score_address:                  0x%08x_%08x\n", H, L);
    printf("        seq_length:                     %u\n", info->seq_length);

    // Group Information
    printf("    Group Information\n");
    printf("        group.this_grid_id:             %u\n", info->group.this_grid_id);
    // GridSyncGroupInfo skipped

    // Cluster Information
    printf("    Cluster Information\n");
    // FlexPosition skipped
    printf("        cluster_global_id:              %u\n", info->cluster_global_id);
    printf("        cluster_in_group_id:            %u\n", info->cluster_in_group_id);
    printf("        cluster_in_group_id_x:          %u\n", info->cluster_in_group_id_x);
    printf("        cluster_in_group_id_y:          %u\n", info->cluster_in_group_id_y);
    printf("        cluster_active:                 %u\n", info->cluster_active);

    // Tiling Information
    printf("    Tiling Information\n");
    printf("        S:                              %u\n", info->S);
    printf("        H:                              %u\n", info->H);
    printf("        E:                              %u\n", info->E);

    // L1 Location Information
    printf("    L1 Location Information\n");
    printf("        L1_I1:                          0x%08x\n", info->L1_I1);
    printf("        L1_W1:                          0x%08x\n", info->L1_W1);
    printf("        L1_ID:                          0x%08x\n", info->L1_ID);
    printf("        L1_VA:                          0x%08x\n", info->L1_VA);
    printf("        L1_area:                        0x%08x\n", info->L1_area);
}


// DeepSeekMoEGateInfo DeepSeek_MoE_gate_analyze(
//     uint64_t                input_address,
//     uint64_t                weight_address,
//     uint32_t                seq_length){

//     DeepSeekMoEGateInfo info;

//     info.input_address      = input_address;
//     info.weight_address     = weight_address;
//     info.seq_length         = seq_length;

//     FlexPosition pos        = get_pos(flex_get_cluster_id());
//     info.cluster_pos        = pos;
//     info.cluster_global_id  = flex_get_cluster_id();

//     uint32_t cutoff_x       = (seq_length + DEEPSEEK_MOE_GATE_S - 1) / DEEPSEEK_MOE_GATE_S;
//     info.cluster_active     = pos.x < cutoff_x ? 1 : 0;

//     info.S                  = (pos.x == cutoff_x - 1)? seq_length - pos.x * DEEPSEEK_MOE_GATE_S : DEEPSEEK_MOE_GATE_S;
//     info.H                  = DEEPSEEK_MOE_GATE_H;
//     info.E                  = DEEPSEEK_MOE_ROUTED_EXPERT;

//     info.num_k              = (DEEPSEEK_EMBEDDED_LENGTH + DEEPSEEK_MOE_GATE_H * ARCH_NUM_CLUSTER_Y - 1) / (DEEPSEEK_MOE_GATE_H * ARCH_NUM_CLUSTER_Y);

//     return info;
// }

void DeepSeek_MoE_gate_C32x32_run(DeepSeekMoEGateInfo * info);

void DeepSeek_MoE_gate_C32x32(
    uint64_t                        input_address,
    uint64_t                        weight_address,
    uint64_t                        index_address,
    uint64_t                        score_address,
    uint32_t                        seq_length)
{
    DeepSeekMoEGateInfo info;

    info.input_address              = input_address;
    info.weight_address             = weight_address;
    info.index_address              = index_address;
    info.score_address              = score_address;
    info.seq_length                 = seq_length;

    //Group infomation
    FlexPosition pos                = get_pos(flex_get_cluster_id());
    info.cluster_pos                = pos;
    info.cluster_global_id          = flex_get_cluster_id();
    info.group                      = grid_sync_group_init(32,2);
    info.cluster_in_group_id_x      = pos.x % info.group.grid_x_dim;
    info.cluster_in_group_id_y      = pos.y % info.group.grid_y_dim;
    info.cluster_in_group_id        = info.cluster_in_group_id_x + info.group.this_grid_cluster_num_x * info.cluster_in_group_id_y;
    info.cluster_active             = info.cluster_in_group_id < 56 ? 1 : 0;

    info.S                          = seq_length / 16;
    info.H                          = DEEPSEEK_MOE_GATE_H;
    info.E                          = DEEPSEEK_MOE_ROUTED_EXPERT;

    info.L1_I_size                  = info.S * info.H * DATA_TYPE_BYTE;
    info.L1_W_size                  = info.E * info.H * DATA_TYPE_BYTE;
    info.L1_O_size                  = info.S * info.E * DATA_TYPE_BYTE;

    info.L1_I1                      = local(0);
    info.L1_W1                      = info.L1_I1 + info.L1_I_size;
    info.L1_ID                      = info.L1_W1 + info.L1_W_size;
    info.L1_VA                      = info.L1_ID + info.L1_O_size;
    info.L1_area                    = info.L1_VA + info.L1_O_size;

    // for (int cid = 0; cid < 1; ++cid)
    // {
    //     if (flex_get_core_id() == 0 && flex_get_cluster_id() == cid)
    //     {
    //         printf("[Cluster %3d] ---------------------------------------\n", cid);
    //         DeepSeek_MoE_gate_print_info(&info);
    //     }
    //     flex_global_barrier_xy();//Global barrier
    // }
    // flex_global_barrier_xy();

    // if (info.L1_area > ARCH_CLUSTER_TCDM_SIZE)
    // {
    //     if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) printf("[L1 area overflow] L1 size: 0x%08x, planned size: 0x%08x\n", ARCH_CLUSTER_TCDM_SIZE, info.L1_area);
    //     return;
    // }

    
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    DeepSeek_MoE_gate_C32x32_run(&info);
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

}


void DeepSeek_MoE_gate_C32x32_run(DeepSeekMoEGateInfo * info)
{
    //1. load and multicast
    if (info->group.this_grid_id == 0 && info->cluster_active == 1 && flex_is_dm_core())
    {
        //load Weight
        flex_dma_async_2d(
            info->L1_W1, /*destination*/
            info->weight_address + info->cluster_in_group_id * info->L1_W_size, /*source*/
            info->E * DATA_TYPE_BYTE, /*transfer size*/
            info->E * DATA_TYPE_BYTE, /*destination stride*/
            info->E * DATA_TYPE_BYTE, /*source stride*/
            info->H /*repeat*/); //Start 2D iDMA
        flex_dma_async_wait_all(); // Wait for iDMA Finishing
        //col-wise multicast
        flex_dma_async_broadcast(
            info->L1_W1/*dst_offset*/,
            info->L1_W1/*src_offset*/,
            info->L1_W_size/*transfer_size*/,
            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
            ~(info->group.wakeup_col_mask)/*col_mask*/);
        flex_dma_async_wait_all(); // Wait for iDMA Finishing
    }
    flex_global_barrier_xy();

    // 2. load input and compute
    if (info->cluster_active == 1)
    {
        if (flex_is_dm_core())
        {
            flex_dma_async_2d(
                info->L1_I1, /*destination*/
                info->input_address + info->group.this_grid_id * DEEPSEEK_MOE_GATE_K * info->L1_I_size + info->cluster_in_group_id * DEEPSEEK_MOE_GATE_H * DATA_TYPE_BYTE, /*source*/
                info->H * DATA_TYPE_BYTE, /*transfer size*/
                info->H * DATA_TYPE_BYTE, /*destination stride*/
                DEEPSEEK_EMBEDDED_LENGTH * DATA_TYPE_BYTE, /*source stride*/
                info->S /*repeat*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
        flex_intra_cluster_sync();
        if (flex_is_first_core())
        {
            flex_redmule_config(info->S, info->H, info->E);
            flex_redmule_trigger(info->L1_I1, info->L1_W1, info->L1_ID, REDMULE_COMPUTE_TYPE);
            flex_redmule_wait();
        }
    }
    grid_sync_group_barrier_xy(&(info->group));

    //3. Redcution, Transpose, TopK
    if (info->cluster_in_group_id == info->group.this_grid_id)
    {
        if (flex_is_dm_core())
        {
            //Reduction
            flex_dma_async_reduction(
                info->L1_ID/*dst_offset*/,
                info->L1_ID/*src_offset*/,
                info->L1_I_size/*transfer_size*/,
                COLLECTIVE_REDSUM_TYPE/*fmt*/,
                info->group.wakeup_row_mask/*row_mask*/,
                info->group.wakeup_col_mask/*col_mask*/);
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
        flex_intra_cluster_sync();
        if (flex_is_first_core())
        {
            //Transpose
            flex_transpose_engine_config(info->S,info->E,info->L1_ID,info->L1_VA,DATA_TYPE_BYTE);
            flex_transpose_engine_trigger();
            flex_transpose_engine_wait();
            //Prepare ID
            //TopK
            vector_lib_top_k(info->L1_VA,info->L1_ID,info->S,info->E,DEEPSEEK_MOE_ACTIVE_EXPERT);
        }
        flex_intra_cluster_sync();
        if (flex_is_dm_core())
        {
            //Store index
            flex_dma_async_2d(
                info->index_address + info->group.this_grid_id * info->S * DATA_TYPE_BYTE, /*destination*/
                info->L1_ID, /*source*/
                info->S * DATA_TYPE_BYTE, /*transfer size*/
                info->seq_length * DATA_TYPE_BYTE, /*destination stride*/
                info->S * DATA_TYPE_BYTE, /*source stride*/
                DEEPSEEK_MOE_ACTIVE_EXPERT /*repeat*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
            //Store score
            flex_dma_async_2d(
                info->score_address + info->group.this_grid_id * info->S * DATA_TYPE_BYTE, /*destination*/
                info->L1_VA, /*source*/
                info->S * DATA_TYPE_BYTE, /*transfer size*/
                info->seq_length * DATA_TYPE_BYTE, /*destination stride*/
                info->S * DATA_TYPE_BYTE, /*source stride*/
                DEEPSEEK_MOE_ACTIVE_EXPERT /*repeat*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
    }
}

#endif