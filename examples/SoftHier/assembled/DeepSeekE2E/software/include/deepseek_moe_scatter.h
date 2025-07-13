#ifndef _DEEPSEEK_MOE_SCATTER_H_
#define _DEEPSEEK_MOE_SCATTER_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"

#define DEEPSEEK_MOE_SYNC_CNT_OFFSET        64
#define DEEPSEEK_MOE_SCATTER_ENABLE_ROW     2

typedef struct DeepSeekMoEScatterInfo
{
    //General information
    uint64_t                        input_address;
    uint64_t                        index_address;
    uint64_t                        locat_address;
    uint64_t                        scatter_address;
    uint32_t                        seq_length;
    uint64_t                        scatter_expert_interval;

    //Cluster information
    FlexPosition                    cluster_pos;
    uint32_t                        cluster_global_id;
    uint32_t                        cluster_active;

    //Tiling information
    uint32_t                        S;
    uint32_t                        E;

    //Usefull parameters for address calculation
    uint32_t                        L1_ID_size;
    uint32_t                        L1_I_size;
    volatile uint32_t *             SYNC_cnt_ptr;

    //L1 location information
    uint32_t                        L1_ID;
    uint32_t                        L1_LC;
    uint32_t                        L1_I1;
    uint32_t                        L1_area;

}DeepSeekMoEScatterInfo;


void DeepSeek_MoE_scatter_print_info(DeepSeekMoEScatterInfo *info) {
    uint32_t H, L;

    // General Information
    printf("    General Information\n");
    H = info->input_address >> 32;
    L = (uint32_t)(info->input_address);
    printf("        input_address:                  0x%08x_%08x\n", H, L);
    H = info->index_address >> 32;
    L = (uint32_t)(info->index_address);
    printf("        index_address:                  0x%08x_%08x\n", H, L);
    H = info->locat_address >> 32;
    L = (uint32_t)(info->locat_address);
    printf("        locat_address:                  0x%08x_%08x\n", H, L);
    H = info->scatter_address >> 32;
    L = (uint32_t)(info->scatter_address);
    printf("        scatter_address:                0x%08x_%08x\n", H, L);
    H = info->scatter_expert_interval >> 32;
    L = (uint32_t)(info->scatter_expert_interval);
    printf("        scatter_expert_interval:        0x%08x_%08x\n", H, L);
    printf("        seq_length:                     %u\n", info->seq_length);

    // Cluster Information
    printf("    Cluster Information\n");
    // FlexPosition skipped
    printf("        cluster_global_id:              %u\n", info->cluster_global_id);
    printf("        cluster_active:                 %u\n", info->cluster_active);

    // Tiling Information
    printf("    Tiling Information\n");
    printf("        S:                              %u\n", info->S);
    printf("        E:                              %u\n", info->E);

    // L1 Location Information
    printf("    L1 Location Information\n");
    printf("        L1_ID:                          0x%08x\n", info->L1_ID);
    printf("        L1_I1:                          0x%08x\n", info->L1_I1);
    printf("        L1_area:                        0x%08x\n", info->L1_area);
}

void DeepSeek_MoE_scatter_run(DeepSeekMoEScatterInfo * info);

void DeepSeek_MoE_scatter(
    uint64_t                        input_address,
    uint64_t                        index_address,
    uint64_t                        locat_address,
    uint64_t                        scatter_address,
    uint32_t                        seq_length)
{
    DeepSeekMoEScatterInfo info;

    info.input_address              = input_address;
    info.index_address              = index_address;
    info.locat_address              = locat_address;
    info.scatter_address            = scatter_address;
    info.seq_length                 = seq_length;
    info.scatter_expert_interval    = seq_length * DEEPSEEK_EMBEDDED_LENGTH * DATA_TYPE_BYTE;

    //Group infomation
    FlexPosition pos                = get_pos(flex_get_cluster_id());
    info.cluster_pos                = pos;
    info.cluster_global_id          = flex_get_cluster_id();

    info.S                          = seq_length <= info.cluster_global_id ? 0 : (1 + (seq_length - info.cluster_global_id - 1) / (DEEPSEEK_MOE_SCATTER_ENABLE_ROW * ARCH_NUM_CLUSTER_X));
    info.cluster_active             = (pos.y < DEEPSEEK_MOE_SCATTER_ENABLE_ROW) && (info.S > 0);
    info.E                          = DEEPSEEK_MOE_ACTIVE_EXPERT;

    info.L1_ID_size                 = info.S * info.E * DATA_TYPE_BYTE;
    info.L1_I_size                  = DEEPSEEK_EMBEDDED_LENGTH * DATA_TYPE_BYTE;
    info.SYNC_cnt_ptr               = (volatile uint32_t *) (ARCH_SYNC_BASE+(flex_get_cluster_id()*ARCH_SYNC_SIZE)+DEEPSEEK_MOE_SYNC_CNT_OFFSET);
    info.L1_ID                      = local(0);
    info.L1_LC                      = info.L1_ID + info.L1_ID_size;
    info.L1_I1                      = info.L1_LC + info.L1_ID_size;
    info.L1_area                    = info.L1_I1 + info.L1_I_size;

    //Reset sync counter
    *info.SYNC_cnt_ptr              = 0;

    // for (int cid = 0; cid < 70; ++cid)
    // {
    //     if (flex_get_core_id() == 0 && flex_get_cluster_id() == cid)
    //     {
    //         printf("[Cluster %3d] ---------------------------------------\n", cid);
    //         DeepSeek_MoE_scatter_print_info(&info);
    //     }
    //     flex_global_barrier_xy();//Global barrier
    // }
    // flex_global_barrier_xy();

    if (info.L1_area > ARCH_CLUSTER_TCDM_SIZE)
    {
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) printf("[L1 area overflow] L1 size: 0x%08x, planned size: 0x%08x\n", ARCH_CLUSTER_TCDM_SIZE, info.L1_area);
        return;
    }

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    DeepSeek_MoE_scatter_run(&info);
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();
}


void DeepSeek_MoE_scatter_run(DeepSeekMoEScatterInfo * info)
{
    if (info->cluster_active && flex_is_dm_core())
    {
        //1. load index
        flex_dma_async_2d(
            info->L1_ID, /*destination*/
            info->index_address + info->cluster_global_id * info->S * DATA_TYPE_BYTE, /*source*/
            info->S * DATA_TYPE_BYTE, /*transfer size*/
            info->S * DATA_TYPE_BYTE, /*destination stride*/
            info->seq_length * DATA_TYPE_BYTE, /*source stride*/
            info->E /*repeat*/); //Start 2D iDMA
        flex_dma_async_wait_all(); // Wait for iDMA Finishing

        for (int s = 0; s < info->S; ++s)
        {
            //2. load input
            flex_dma_async_1d(
                info->L1_I1, /*destination*/
                info->input_address + info->cluster_global_id * info->S * DEEPSEEK_EMBEDDED_LENGTH * DATA_TYPE_BYTE, /*source*/
                DEEPSEEK_EMBEDDED_LENGTH * DATA_TYPE_BYTE/*transfer size*/); //Start 1D iDMA

            for (int e = 0; e < info->E; ++e)
            {
                //3. got expert index
                deepseek_data_t expert_idx = ((s << 3) + e + info->cluster_global_id) % DEEPSEEK_MOE_ROUTED_EXPERT;

                //4. got store index
                uint32_t * sync_ptr = (volatile uint32_t *) (ARCH_SYNC_BASE+(expert_idx*ARCH_SYNC_SIZE)+DEEPSEEK_MOE_SYNC_CNT_OFFSET);
                deepseek_data_t store_idx = flex_amo_fetch_add(sync_ptr);
                deepseek_data_t * locat_ptr = (deepseek_data_t *) info->L1_LC;
                locat_ptr[e*info->S + s] = store_idx;

                //5. Calculate scatter address
                uint64_t dest_addr = info->scatter_address + expert_idx * info->scatter_expert_interval + store_idx * DEEPSEEK_EMBEDDED_LENGTH * DATA_TYPE_BYTE;

                flex_dma_async_wait_all(); // Wait for previouse iDMA Finishing

                //6. scatter input
                flex_dma_async_1d(
                    dest_addr, /*destination*/
                    info->L1_I1, /*source*/
                    DEEPSEEK_EMBEDDED_LENGTH * DATA_TYPE_BYTE/*transfer size*/); //Start 1D iDMA
            }

            flex_dma_async_wait_all(); // Wait for previouse iDMA Finishing
        }

        //7. store locate matrix
        flex_dma_async_2d(
            info->locat_address + info->cluster_global_id * info->S * DATA_TYPE_BYTE, /*destination*/
            info->L1_LC, /*source*/
            info->S * DATA_TYPE_BYTE, /*transfer size*/
            info->seq_length * DATA_TYPE_BYTE, /*destination stride*/
            info->S * DATA_TYPE_BYTE, /*source stride*/
            info->E /*repeat*/); //Start 2D iDMA
        flex_dma_async_wait_all(); // Wait for iDMA Finishing
    }
}

#endif