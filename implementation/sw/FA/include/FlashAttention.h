// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 6.Jan.2025

// Note: Applied Layout Example
/*
    Q,K,V,O:
    __|<--head dimension-->|
     ^      ^              |    | <q seq> * <speculative length> * <q per head group>
     |      | head group 0 --> {
     |      V              |    | <kv seq>
    batch 0 ---------------|
     |      ^              |
     |      | head group 1 |
     v      v              |
    -----------------------|
     ^      ^              |
     |      | head group 0 |
     |      v              |
    batch 1 ---------------|
     |      ^              |
     |      | head group 1 |
     v      v              |
    -----------------------|
*/

#ifndef _FLASH_ATTENTION_H_
#define _FLASH_ATTENTION_H_

#include "FlashAttentionUtil.h"

void flash_attention_v2_run(FlashAttentionInfo* info);
void flash_attention_v2_async_run(FlashAttentionInfo* info);

int flash_attention(
    uint32_t kv_sequence_length,
    uint32_t q_sequence_length,
    uint32_t speculative_length,
    uint32_t head_dimemsion,
    uint32_t num_head,
    uint32_t num_head_group,
    uint32_t batch_size,
    uint32_t shape_x,
    uint32_t shape_y,
    uint64_t Q_base_address,
    uint64_t K_base_address,
    uint64_t V_base_address,
    uint64_t O_base_address,
    uint32_t async_enable){

    //analyze flash attention settings
    flex_global_barrier_xy();
    FlashAttentionInfo info = flash_attention_analyze(
                                    kv_sequence_length,
                                    q_sequence_length,
                                    speculative_length,
                                    head_dimemsion,
                                    num_head,
                                    num_head_group,
                                    batch_size,
                                    shape_x,
                                    shape_y,
                                    Q_base_address,
                                    K_base_address,
                                    V_base_address,
                                    O_base_address);
    flex_global_barrier_xy();

    //execute flash attention
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    if (async_enable)
    {
        if (info.work_group_enable) flash_attention_v2_async_run(&info);
    } else {
        if (info.work_group_enable) flash_attention_v2_run(&info);
    }
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

    return 0;
}


void flash_attention_v2_run(FlashAttentionInfo* info){
    for (int heads = 0; heads < info->work_group_head_num; ++heads)
    {
        for (int i = 0; i < info->Tr; ++i)
        {
            //load Q from HBM
            if (flex_is_dm_core()){
                flex_record_start_hbmAccess();
                flex_dma_async_1d(
                    info->L1_Q,
                    info->HBM_Q + heads * info->heads_QO_size/*head offset*/ + i * info->block_QO_size/*i block offset*/,
                    info->L1_Q_size);
                flex_dma_async_wait_all();
                flex_record_end_hbmAccess();
            }

            //Initialize O
            if (flex_is_dm_core())
            {
                flex_dma_async_1d(info->L1_O,zomem(0),info->L1_O_size);
                flex_dma_async_wait_all();
            }
            flex_intra_cluster_sync();
                
            for (int j = 0; j < info->Tc; ++j)
            {
                if (flex_is_dm_core()){
                    //load K from HBM
                    flex_record_start_hbmAccess();
                    flex_dma_async_1d(
                        info->L1_KT,
                        info->HBM_K + heads * info->heads_KTV_size/*head offset*/ + j * info->block_KTV_size/*j block offset*/,
                        info->L1_KT_size);
                    flex_dma_async_wait_all();
                    //load V from HBM
                    flex_dma_async_1d(
                        info->L1_V,
                        info->HBM_V + heads * info->heads_KTV_size/*head offset*/ + j * info->block_KTV_size/*j block offset*/,
                        info->L1_V_size);
                    flex_dma_async_wait_all();
                    flex_record_end_hbmAccess();
                    //transpose K
                    flex_transpose_engine_config(info->Bc,info->d,info->L1_KT,info->L1_KT,DATA_TYPE_BYTE);
                    flex_transpose_engine_trigger();
                    flex_transpose_engine_wait();
                    //Initialize Att
                    flex_dma_async_1d(info->L1_A,zomem(0),info->L1_A_size);
                    flex_dma_async_wait_all();
                }

                flex_intra_cluster_sync();

                //Compute Att = Q*Kt
                if (flex_is_first_core())
                {
                    //Att = (Qi_sy * KTj_sx)
                    flex_redmule_config(info->Br, info->d, info->Bc);
                    flex_redmule_trigger(
                        info->L1_Q,
                        info->L1_KT,
                        info->L1_A,
                        REDMULE_COMPUTE_TYPE);
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();

                //Compute m = max(mr, rowmax(Att))
                if (info->spatz_attached)
                {
                    if (flex_is_first_core()) flex_record_start_spatz();
                    flat_attention_vector_rowmax_MV_V(
                        info->Bc,
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_A,
                        info->L1SP_mr,
                        info->L1SP_m,
                        info->sqrt_dim
                        );
                    if (flex_is_first_core()) flex_record_end_spatz();
                }
                flex_intra_cluster_sync();

                if (info->spatz_attached)
                {
                    if (flex_is_first_core()) flex_record_start_spatz();
                    //p = exp(att - m)
                    flat_attention_vector_EXP_MV(
                        info->Bc,
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_A,
                        info->L1SP_m
                        );

                    //l = rowsum(p)
                    flat_attention_vector_rowsum_M_V(
                        info->Bc,
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_A,
                        info->L1SP_l
                        );
                    if (flex_is_first_core()) flex_record_end_spatz();
                }
                flex_intra_cluster_sync();

                if (info->spatz_attached && j != 0)
                {
                    if (flex_is_first_core()) flex_record_start_spatz();
                    //Compute e = exp(mr - m)
                    flat_attention_vector_EXP_VV_V(
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_mr,
                        info->L1SP_m,
                        info->L1SP_e);

                    //Compute l = e-*-lr + l
                    flat_attention_vector_update_l(
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_lr,
                        info->L1SP_e,
                        info->L1SP_l);

                    //Compute O = O/e
                    flat_attention_vector_M_mul_V(
                        info->d,
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O,
                        info->L1SP_e);
                    if (flex_is_first_core()) flex_record_end_spatz();
                }
                flex_intra_cluster_sync();

                //Compute O = O + p*Vj
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br, info->Bc, info->d);
                    flex_redmule_trigger(
                        info->L1_A,
                        info->L1_V,
                        info->L1_O,
                        REDMULE_COMPUTE_TYPE);
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();

                //Update mr, lr
                if (flex_is_dm_core())
                {
                    flex_dma_async_1d(info->L1_mr,info->L1_m,info->L1_m_size);
                    flex_dma_async_1d(info->L1_lr,info->L1_l,info->L1_l_size);
                    flex_dma_async_wait_all();
                }
                flex_intra_cluster_sync();
            }

            if (info->spatz_attached){
                if (flex_is_first_core()) flex_record_start_spatz();
                //O = O/l
                flat_attention_vector_M_div_V(
                    info->d,
                    info->Br / ARCH_SPATZ_ATTACED_CORES,
                    info->L1SP_O,
                    info->L1SP_l
                    );
                if (flex_is_first_core()) flex_record_end_spatz();
            }
            flex_intra_cluster_sync();

            //Store O
            if (flex_is_dm_core())
            {
                flex_record_start_hbmAccess();
                flex_dma_async_1d(
                    info->HBM_O + heads * info->heads_QO_size/*head offset*/ + i * info->block_QO_size/*i block offset*/,
                    info->L1_O,
                    info->L1_O_size);
                flex_dma_async_wait_all();
                flex_record_end_hbmAccess();
            }
            flex_intra_cluster_sync();
        }
    }
}









void flash_attention_v2_async_run(FlashAttentionInfo* info){
    for (int heads = 0; heads < info->work_group_head_num; ++heads)
    {
        for (int i = 0; i < info->Tr; ++i)
        {
            //Initialize O
            if (flex_is_dm_core())
            {
                flex_dma_async_1d(info->L1_O,zomem(0),info->L1_O_size);
                flex_dma_async_wait_all();
            }

            //load Q from HBM
            if (flex_is_dm_core()){
                flex_record_start_hbmAccess();
                flex_dma_async_1d(
                    info->L1_Q,
                    info->HBM_Q + heads * info->heads_QO_size/*head offset*/ + i * info->block_QO_size/*i block offset*/,
                    info->L1_Q_size);
                flex_dma_async_wait_all();
                flex_record_end_hbmAccess();
            }

            //load K0 from HBM
            if (flex_is_dm_core()){
                flex_record_start_hbmAccess();
                flex_dma_async_1d(
                    info->L1_KT,
                    info->HBM_K + heads * info->heads_KTV_size/*head offset*/ + 0 * info->block_KTV_size/*j block offset*/,
                    info->L1_KT_size);
                flex_dma_async_wait_all();
                flex_record_end_hbmAccess();
                //transpose K
                flex_transpose_engine_config(info->Bc,info->d,info->L1_KT,info->L1_KT,DATA_TYPE_BYTE);
                flex_transpose_engine_trigger();
                flex_transpose_engine_wait();
            }
            flex_intra_cluster_sync();

            if (flex_is_first_core())
            {
                //Compute Att = Q*Kt using RedMule. Commit and wait
                flex_redmule_config(info->Br, info->d, info->Bc);
                flex_redmule_trigger(
                    info->L1_Q,
                    info->L1_KT,
                    info->L1_A,
                    REDMULE_COMPUTE_TYPE);
                flex_redmule_wait();
            }
            flex_intra_cluster_sync();

            //Compute m = max(mr, rowmax(Att))
            if (info->spatz_attached)
            {
                if (flex_is_first_core()) flex_record_start_spatz();
                flat_attention_vector_rowmax_MV_V(
                    info->Bc,
                    info->Br / ARCH_SPATZ_ATTACED_CORES,
                    info->L1SP_A,
                    info->L1SP_mr,
                    info->L1SP_m,
                    info->sqrt_dim
                    );
                if (flex_is_first_core()) flex_record_end_spatz();
            }
            flex_intra_cluster_sync();

            if (info->spatz_attached)
            {
                if (flex_is_first_core()) flex_record_start_spatz();
                //p = exp(att - m)
                flat_attention_vector_EXP_MV(
                    info->Bc,
                    info->Br / ARCH_SPATZ_ATTACED_CORES,
                    info->L1SP_A,
                    info->L1SP_m
                    );

                //l = rowsum(p)
                flat_attention_vector_rowsum_M_V(
                    info->Bc,
                    info->Br / ARCH_SPATZ_ATTACED_CORES,
                    info->L1SP_A,
                    info->L1SP_l
                    );
                if (flex_is_first_core()) flex_record_end_spatz();
            }
            flex_intra_cluster_sync();

                
            for (int j = 1; j < info->Tc; ++j)
            {
                if (flex_is_dm_core()){
                    //load K from HBM
                    flex_record_start_hbmAccess();
                    flex_dma_async_1d(
                        info->L1_KT,
                        info->HBM_K + heads * info->heads_KTV_size/*head offset*/ + j * info->block_KTV_size/*j block offset*/,
                        info->L1_KT_size);
                    flex_dma_async_wait_all();
                    flex_record_end_hbmAccess();
                    //transpose K
                    flex_transpose_engine_config(info->Bc,info->d,info->L1_KT,info->L1_KT,DATA_TYPE_BYTE);
                    flex_transpose_engine_trigger();
                    flex_transpose_engine_wait();
                    //Initialize Att
                    flex_dma_async_1d(info->L1_A,zomem(0),info->L1_A_size);
                    flex_dma_async_wait_all();
                }
                flex_intra_cluster_sync();

                if (flex_is_first_core())
                {
                    //Compute S next = Q i K Tj using RedMule. Commit but do not wait
                    flex_redmule_config(info->Br, info->d, info->Bc);
                    flex_redmule_trigger(
                        info->L1_Q,
                        info->L1_KT,
                        info->DB_L1_A,
                        REDMULE_COMPUTE_TYPE);
                }

                if (flex_is_dm_core()){
                    //load V(j-1) from HBM
                    flex_record_start_hbmAccess();
                    flex_dma_async_1d(
                        info->L1_V,
                        info->HBM_V + heads * info->heads_KTV_size/*head offset*/ + (j-1) * info->block_KTV_size/*j block offset*/,
                        info->L1_V_size);
                    flex_dma_async_wait_all();
                    flex_record_end_hbmAccess();
                }

                flex_intra_cluster_sync();

                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                    //Compute O i = O i + P̃ cur V j − 1 using WGMMA. Commit but do not wait
                    flex_redmule_config(info->Br, info->Bc, info->d);
                    flex_redmule_trigger(
                        info->L1_A,
                        info->L1_V,
                        info->L1_O,
                        REDMULE_COMPUTE_TYPE);
                }
                flex_intra_cluster_sync();

                //Compute m = max(mr, rowmax(Att))
                if (info->spatz_attached)
                {
                    if (flex_is_first_core()) flex_record_start_spatz();
                    flat_attention_vector_rowmax_MV_V(
                        info->Bc,
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->DB_L1SP_A,
                        info->L1SP_mr,
                        info->L1SP_m,
                        info->sqrt_dim
                        );
                    if (flex_is_first_core()) flex_record_end_spatz();
                }
                flex_intra_cluster_sync();

                if (info->spatz_attached)
                {
                    if (flex_is_first_core()) flex_record_start_spatz();
                    //p = exp(att - m)
                    flat_attention_vector_EXP_MV(
                        info->Bc,
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->DB_L1SP_A,
                        info->L1SP_m
                        );

                    //l = rowsum(p)
                    flat_attention_vector_rowsum_M_V(
                        info->Bc,
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->DB_L1SP_A,
                        info->L1SP_l
                        );
                    if (flex_is_first_core()) flex_record_end_spatz();
                }
                flex_intra_cluster_sync();

                if (info->spatz_attached && j != 0)
                {
                    if (flex_is_first_core()) flex_record_start_spatz();
                    //Compute e = exp(mr - m)
                    flat_attention_vector_EXP_VV_V(
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_mr,
                        info->L1SP_m,
                        info->L1SP_e);

                    //Compute l = e-*-lr + l
                    flat_attention_vector_update_l(
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_lr,
                        info->L1SP_e,
                        info->L1SP_l);
                    if (flex_is_first_core()) flex_record_end_spatz();
                }
                flex_intra_cluster_sync();

                if (flex_is_first_core()) {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();

                if (info->spatz_attached && j != 0)
                {
                    if (flex_is_first_core()) flex_record_start_spatz();
                    //Compute O = O/e
                    flat_attention_vector_M_mul_V(
                        info->d,
                        info->Br / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O,
                        info->L1SP_e);
                    if (flex_is_first_core()) flex_record_end_spatz();
                }
                flex_intra_cluster_sync();

                //Update mr, lr, L1_A
                if (flex_is_dm_core())
                {
                    flex_dma_async_1d(info->L1_mr,info->L1_m,info->L1_m_size);
                    flex_dma_async_wait_all();
                    flex_dma_async_1d(info->L1_lr,info->L1_l,info->L1_l_size);
                    flex_dma_async_wait_all();
                    flex_dma_async_1d(info->L1_A,info->DB_L1_A,info->L1_A_size);
                    flex_dma_async_wait_all();
                }
                flex_intra_cluster_sync();
            }

            //Wait for V T c − 1 to be loaded in shared memory.
            if (flex_is_dm_core())
            {
                //load V(Tc-1) from HBM
                flex_record_start_hbmAccess();
                flex_dma_async_1d(
                    info->L1_V,
                    info->HBM_V + heads * info->heads_KTV_size/*head offset*/ + (info->Tc - 1) * info->block_KTV_size/*j block offset*/,
                    info->L1_V_size);
                flex_dma_async_wait_all();
                flex_record_end_hbmAccess();
            }
            flex_intra_cluster_sync();

            if (flex_is_first_core())
            {
                //Compute O i = O i + P̃ last V T c − 1 using WGMMA. Commit and wait.
                flex_redmule_config(info->Br, info->Bc, info->d);
                flex_redmule_trigger(
                    info->L1_A,
                    info->L1_V,
                    info->L1_O,
                    REDMULE_COMPUTE_TYPE);
                flex_redmule_wait();
            }
            flex_intra_cluster_sync();

            if (info->spatz_attached){
                if (flex_is_first_core()) flex_record_start_spatz();
                //O = O/l
                flat_attention_vector_M_div_V(
                    info->d,
                    info->Br / ARCH_SPATZ_ATTACED_CORES,
                    info->L1SP_O,
                    info->L1SP_l
                    );
                if (flex_is_first_core()) flex_record_end_spatz();
            }
            flex_intra_cluster_sync();

            //Store O
            if (flex_is_dm_core())
            {
                flex_record_start_hbmAccess();
                flex_dma_async_1d(
                    info->HBM_O + heads * info->heads_QO_size/*head offset*/ + i * info->block_QO_size/*i block offset*/,
                    info->L1_O,
                    info->L1_O_size);
                flex_dma_async_wait_all();
                flex_record_end_hbmAccess();
            }
            flex_intra_cluster_sync();
        }
    }
}

#endif
