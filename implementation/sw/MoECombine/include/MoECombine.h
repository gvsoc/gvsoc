#ifndef _MOECOMBINE_H_
#define _MOECOMBINE_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_libfp8.h"
#include "flex_libfp16.h"
#include "moec.h"


typedef struct MoECombineInfo
{
    uint64_t                            input_address;
    uint64_t                            output_address;
    uint64_t                            val_address;
    uint64_t                            idx_address;
    uint64_t                            pos_address;
    uint32_t                            num_total_token;
    uint32_t                            embedded_length;
    uint32_t                            num_routed_experts;
    uint32_t                            num_active_experts;
    uint32_t                            token_per_cluster;
    uint32_t                            start_token;
    uint32_t                            L1_I1;
    uint32_t                            L1_I2;
    uint32_t                            L1_O;
    uint32_t                            L1_V;
    uint32_t                            L1_D;
    uint32_t                            L1_P;
    uint32_t                            L1_Area;
    uint32_t                            L1_I_size;
    uint32_t                            L1_V_size;
    uint32_t                            L1_D_size;
    uint32_t                            L1_P_size;
    uint64_t                            expert_buffer_gap;

    //Spatz Information
    uint32_t                            spatz_num;
    uint32_t                            spatz_check_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t                            spatz_sid_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t                            spatz_attached;
    uint32_t                            spatz_sid;

    //L1 location for spatz partition
    uint32_t                            L1SP_I1;
    uint32_t                            L1SP_I2;
    uint32_t                            L1SP_O;

}MoECombineInfo;

MoECombineInfo MoECombineAnaylze(
    uint32_t num_total_token,
    uint32_t embedded_length,
    uint32_t num_routed_experts,
    uint32_t num_active_experts,
    uint32_t token_per_cluster,
    uint64_t input_address,
    uint64_t output_address,
    uint64_t val_address,
    uint64_t idx_address,
    uint64_t pos_address)
{
    MoECombineInfo info;

    info.input_address                  = input_address;
    info.output_address                 = output_address;
    info.val_address                    = val_address;
    info.idx_address                    = idx_address;
    info.pos_address                    = pos_address;
    info.num_total_token                = num_total_token;
    info.embedded_length                = embedded_length;
    info.num_routed_experts             = num_routed_experts;
    info.num_active_experts             = num_active_experts;
    info.token_per_cluster              = token_per_cluster;
    FlexPosition pos                    = get_pos(flex_get_cluster_id());
    info.start_token                    = (((pos.x + pos.y) % ARCH_NUM_CLUSTER_X) * ARCH_NUM_CLUSTER_Y + pos.y) * info.token_per_cluster;
    info.L1_I1                          = local(0);
    info.L1_I2                          = info.L1_I1 + info.embedded_length * DATA_TYPE_BYTE;
    info.L1_O                           = info.L1_I2 + info.embedded_length * DATA_TYPE_BYTE;
    info.L1_V                           = info.L1_O  + info.embedded_length * DATA_TYPE_BYTE;
    info.L1_D                           = info.L1_V  + info.token_per_cluster * info.num_active_experts * DATA_TYPE_BYTE;
    info.L1_P                           = info.L1_D  + info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.L1_Area                        = info.L1_P  + info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.L1_I_size                      = info.embedded_length * DATA_TYPE_BYTE;
    info.L1_V_size                      = info.token_per_cluster * info.num_active_experts * DATA_TYPE_BYTE;
    info.L1_D_size                      = info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.L1_P_size                      = info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.expert_buffer_gap              = info.num_total_token * info.embedded_length * DATA_TYPE_BYTE;

    //Spatz information
    info.spatz_num                      = ARCH_SPATZ_ATTACED_CORES;
    uint32_t temp[ARCH_NUM_CORE_PER_CLUSTER] = ARCH_SPATZ_ATTACED_CHECK_LIST;
    for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i){info.spatz_check_list[i] = temp[i];}
    uint32_t sidt[ARCH_NUM_CORE_PER_CLUSTER] = ARCH_SPATZ_ATTACED_SID_LIST;
    for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i){info.spatz_sid_list[i] = sidt[i];}
    info.spatz_attached                 = info.spatz_check_list[flex_get_core_id()];
    info.spatz_sid                      = info.spatz_sid_list[flex_get_core_id()];

    //L1 location for spatz partition
    info.L1SP_I1                        = info.L1_I1 + info.spatz_sid * (info.L1_I_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_I2                        = info.L1_I2 + info.spatz_sid * (info.L1_I_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_O                         = info.L1_O  + info.spatz_sid * (info.L1_I_size / ARCH_SPATZ_ATTACED_CORES);

    return info;
}

inline void vector_lib_facc(uint32_t i_addr, uint32_t o_addr, moec_data_t scalar, uint32_t vlen);

void MoECombineRun(MoECombineInfo * info)
{
    while (info->start_token < info->num_total_token)
    {
        //Do MoECombine
        if (flex_is_dm_core())
        {
            //1. Load vals, indices, positions to L1
            flex_dma_async_1d(
                info->L1_V, /*destination*/
                info->val_address + info->start_token * info->num_active_experts * DATA_TYPE_BYTE, /*source*/
                info->L1_V_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_1d(
                info->L1_D, /*destination*/
                info->idx_address + info->start_token * info->num_active_experts * sizeof(int), /*source*/
                info->L1_D_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_1d(
                info->L1_P, /*destination*/
                info->pos_address + info->start_token * info->num_active_experts * sizeof(int), /*source*/
                info->L1_P_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
            //2. reset output
            flex_dma_async_1d(info->L1_O,zomem(0),info->L1_I_size);
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
        flex_intra_cluster_sync();

        for (int t = 0; t < info->token_per_cluster; ++t)
        {
            volatile moec_data_t *  val_ptr = (volatile moec_data_t *)(info->L1_V);
            volatile uint32_t *     idx_ptr = (volatile uint32_t *)(info->L1_D);
            volatile uint32_t *     pos_ptr = (volatile uint32_t *)(info->L1_P);

            for (int e = 0; e < info->num_active_experts; ++e)
            {
                //3. load input
                if (flex_is_dm_core())
                {
                    uint64_t token_addr = info->input_address + idx_ptr[t * info->num_active_experts + e] * info->expert_buffer_gap + pos_ptr[t * info->num_active_experts + e] * info->L1_I_size;
                    flex_dma_async_1d(
                        info->L1_I1, /*destination*/
                        token_addr, /*source*/
                        info->L1_I_size/*transfer size*/); //Start 1D iDMA
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }
                flex_intra_cluster_sync();

                //4. compute combine
                if (info->spatz_attached)
                {
                    vector_lib_facc(
                        info->L1SP_I1/*i_addr*/,
                        info->L1SP_O/*o_addr*/,
                        val_ptr[t * info->num_active_experts + e]/*scalar*/,
                        info->embedded_length / ARCH_SPATZ_ATTACED_CORES/*vlen*/);
                }
                flex_intra_cluster_sync();
            }

            if (flex_is_dm_core())
            {
                //5. Store back output
                flex_dma_async_1d(
                    info->output_address + (info->start_token + t) * info->L1_I_size, /*destination*/
                    info->L1_O, /*source*/
                    info->L1_I_size/*transfer size*/); //Start 1D iDMA
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                flex_dma_async_1d(info->L1_O,zomem(0),info->L1_I_size);
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
            }
        }

        //Update counter
        info->start_token += ARCH_NUM_CLUSTER * info->token_per_cluster;
    }
}

inline void vector_lib_facc(
    uint32_t i_addr,
    uint32_t o_addr,
    moec_data_t scalar,
    uint32_t vlen)
{
    uint32_t avl;
    asm volatile("fld fa5, (%0)" ::"r"(&scalar));
    while(vlen > 0){
        asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(vlen));
        asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(i_addr));
        asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(o_addr));
        asm volatile("vfmacc.vf v0, fa5, v8");
        asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(o_addr));
        vlen -= avl;
        i_addr += DATA_TYPE_BYTE*avl;
        o_addr += DATA_TYPE_BYTE*avl;
    }
}

#endif