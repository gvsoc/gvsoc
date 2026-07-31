#ifndef _MOEDISPATCH_H_
#define _MOEDISPATCH_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_libfp8.h"
#include "flex_libfp16.h"
#include "moed.h"

#define EXPERT_COUNTER_PER_CLUSTER      ((MOED_NUM_ROUTED_EXPERTS+ARCH_NUM_CLUSTER-1)/ARCH_NUM_CLUSTER)
#define DEEPSEEK_MOE_SYNC_CNT_OFFSET    64
#define EXPERT_COUNTER_PTR(id)          (ARCH_SYNC_BASE+((id/EXPERT_COUNTER_PER_CLUSTER)*ARCH_SYNC_SIZE)+DEEPSEEK_MOE_SYNC_CNT_OFFSET+((id%EXPERT_COUNTER_PER_CLUSTER)*sizeof(int)))

typedef struct MoEDispatchInfo
{
    uint64_t                            input_address;
    uint64_t                            output_address;
    uint64_t                            idx_address;
    uint64_t                            pos_address;
    uint32_t                            num_total_token;
    uint32_t                            embedded_length;
    uint32_t                            num_routed_experts;
    uint32_t                            num_active_experts;
    uint32_t                            token_per_cluster;
    uint32_t                            start_token;
    uint32_t                            L1_I;
    uint32_t                            L1_D;
    uint32_t                            L1_P;
    uint32_t                            L1_Area;
    uint32_t                            L1_I_size;
    uint32_t                            L1_D_size;
    uint32_t                            L1_P_size;
    uint64_t                            expert_buffer_gap;

}MoEDispatchInfo;

MoEDispatchInfo MoEDispatchAnaylze(
    uint32_t num_total_token,
    uint32_t embedded_length,
    uint32_t num_routed_experts,
    uint32_t num_active_experts,
    uint32_t token_per_cluster,
    uint64_t input_address,
    uint64_t output_address,
    uint64_t idx_address,
    uint64_t pos_address)
{
    MoEDispatchInfo info;

    info.input_address                  = input_address;
    info.output_address                 = output_address;
    info.idx_address                    = idx_address;
    info.pos_address                    = pos_address;
    info.num_total_token                = num_total_token;
    info.embedded_length                = embedded_length;
    info.num_routed_experts             = num_routed_experts;
    info.num_active_experts             = num_active_experts;
    info.token_per_cluster              = token_per_cluster;
    FlexPosition pos                    = get_pos(flex_get_cluster_id());
    info.start_token                    = (((pos.x + pos.y) % ARCH_NUM_CLUSTER_X) * ARCH_NUM_CLUSTER_Y + pos.y) * info.token_per_cluster;
    info.L1_I                           = local(0);
    info.L1_D                           = info.L1_I + info.embedded_length * DATA_TYPE_BYTE;
    info.L1_P                           = info.L1_D + info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.L1_Area                        = info.L1_P + info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.L1_I_size                      = info.embedded_length * DATA_TYPE_BYTE;
    info.L1_D_size                      = info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.L1_P_size                      = info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.expert_buffer_gap              = info.num_total_token * info.embedded_length * DATA_TYPE_BYTE;

    //Initialize the counter
    if (flex_is_dm_core())
    {
        for (int i = 0; i < EXPERT_COUNTER_PER_CLUSTER; ++i)
        {
            int expert_id = EXPERT_COUNTER_PER_CLUSTER * flex_get_cluster_id() + i;
            volatile uint32_t * cnt_ptr = (volatile uint32_t *)EXPERT_COUNTER_PTR(expert_id);
            flex_reset_barrier(cnt_ptr);
        }
    }

    return info;
}


void MoEDispatchRun(MoEDispatchInfo * info)
{
    while (info->start_token < info->num_total_token)
    {
        //Do MoEDispatch
        if (flex_is_dm_core())
        {
            //1. Load indices to L1
            flex_dma_async_1d(
                info->L1_D, /*destination*/
                info->idx_address + info->start_token * info->num_active_experts * sizeof(int), /*source*/
                info->L1_D_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing

            for (int t = 0; t < info->token_per_cluster; ++t)
            {
                volatile uint32_t * idx_ptr = (volatile uint32_t *)(info->L1_D);
                volatile uint32_t * pos_ptr = (volatile uint32_t *)(info->L1_P);

#if MOED_NODIS_ENABLE == 0
                 //2. load input
                flex_dma_async_1d(
                    info->L1_I, /*destination*/
                    info->input_address + (info->start_token + t) * info->embedded_length * DATA_TYPE_BYTE, /*source*/
                    info->L1_I_size/*transfer size*/); //Start 1D iDMA
#endif

                for (int e = 0; e < info->num_active_experts; ++e)
                {
                    //3. got expert index
                    uint32_t expert_idx = idx_ptr[t * info->num_active_experts + e];

                    //4. got store index
                    volatile uint32_t * cnt_ptr = (volatile uint32_t *)EXPERT_COUNTER_PTR(expert_idx);
                    uint32_t store_idx = flex_amo_fetch_add(cnt_ptr);
                    pos_ptr[t * info->num_active_experts + e] = store_idx;

#if MOED_NODIS_ENABLE == 0
                    //5. Calculate dispatch address
                    uint64_t dest_addr = info->output_address + expert_idx * info->expert_buffer_gap + store_idx * info->embedded_length * DATA_TYPE_BYTE;

                    flex_dma_async_wait_all(); // Wait for previouse iDMA Finishing

                    //6. dispatch tokens
                    flex_dma_async_1d(
                        dest_addr, /*destination*/
                        info->L1_I, /*source*/
                        info->L1_I_size/*transfer size*/); //Start 1D iDMA
#endif
                }

#if MOED_NODIS_ENABLE == 0
                flex_dma_async_wait_all(); // Wait for previouse iDMA Finishing
#endif
            }

            //7. Store back position matrix
            flex_dma_async_1d(
                info->pos_address + info->start_token * info->num_active_experts * sizeof(int), /*destination*/
                info->L1_P, /*source*/
                info->L1_P_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
        flex_intra_cluster_sync();

        //Update counter
        info->start_token += ARCH_NUM_CLUSTER * info->token_per_cluster;
    }
}

#endif