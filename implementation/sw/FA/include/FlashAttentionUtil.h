// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 21.Mar.2026

#ifndef _FLASHATTENTION_UTIL_H_
#define _FLASHATTENTION_UTIL_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_transpose_engine.h"
#include "flex_libfp16.h"
#include "flex_libfp8.h"
#include "flex_dump.h"
#include "attn.h"

typedef struct FlashAttentionInfo
{
    //Attention General information
    uint32_t                kv_sequence_length;
    uint32_t                q_sequence_length;
    uint32_t                head_dimemsion;
    uint32_t                head_num;
    uint32_t                batch_size;
    uint32_t                elem_size;

    //FlashAttn Settings
    uint32_t                shape_x;
    uint32_t                shape_y;

    //Cluster information
    FlexPosition            cluster_pos;
    uint32_t                cluster_global_id;

    //Spatz Information
    uint32_t                spatz_num;
    uint32_t                spatz_check_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t                spatz_sid_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t                spatz_attached;
    uint32_t                spatz_sid;

    //Tiling information
    uint32_t                d;
    uint32_t                Br;
    uint32_t                Bc;
    uint32_t                Tr;
    uint32_t                Tc;
    attn_data_t             sqrt_dim;

    //L1 location information
    uint32_t                L1_Q;
    uint32_t                L1_KT;
    uint32_t                L1_V;
    uint32_t                L1_O;
    uint32_t                L1_A;
    uint32_t                L1_m;
    uint32_t                L1_l;
    uint32_t                L1_e;
    uint32_t                L1_mr;
    uint32_t                L1_lr;

    uint32_t                DB_L1_Q;
    uint32_t                DB_L1_KT;
    uint32_t                DB_L1_V;
    uint32_t                DB_L1_O;
    uint32_t                DB_L1_A;
    uint32_t                DB_L1_m;
    uint32_t                DB_L1_l;
    uint32_t                DB_L1_e;
    uint32_t                DB_L1_mr;
    uint32_t                DB_L1_lr;

    //Workload information
    uint32_t                work_group_enable;
    uint32_t                work_group_head_num;
    uint32_t                work_group_head_start;
    uint64_t                HBM_Q;
    uint64_t                HBM_K;
    uint64_t                HBM_V;
    uint64_t                HBM_O;

    //Usefull parameters for address calculation
    uint32_t                L1_Q_size;
    uint32_t                L1_KT_size;
    uint32_t                L1_V_size;
    uint32_t                L1_O_size;
    uint32_t                L1_A_size;
    uint32_t                L1_m_size;
    uint32_t                L1_l_size;
    uint32_t                L1_e_size;
    uint32_t                block_KTV_size;
    uint32_t                block_QO_size;
    uint32_t                heads_KTV_size;
    uint32_t                heads_QO_size;

    //L1 location for spatz partition
    uint32_t                L1SP_O;
    uint32_t                L1SP_A;
    uint32_t                L1SP_m;
    uint32_t                L1SP_l;
    uint32_t                L1SP_e;
    uint32_t                L1SP_mr;
    uint32_t                L1SP_lr;

    uint32_t                DB_L1SP_O;
    uint32_t                DB_L1SP_A;
    uint32_t                DB_L1SP_m;
    uint32_t                DB_L1SP_l;
    uint32_t                DB_L1SP_e;
    uint32_t                DB_L1SP_mr;
    uint32_t                DB_L1SP_lr;

}FlashAttentionInfo;



FlashAttentionInfo flash_attention_analyze(
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
    uint64_t O_base_address){

    FlashAttentionInfo info;

    //Attention General information
    info.kv_sequence_length     = kv_sequence_length;
    info.q_sequence_length      = q_sequence_length * speculative_length * (num_head / num_head_group);
    info.head_dimemsion         = head_dimemsion;
    info.head_num               = num_head_group;
    info.batch_size             = batch_size;

    //FlashAttn Settings
    info.shape_x        		= shape_x;
    info.shape_y        		= shape_y;

    //Cluster information
    FlexPosition pos            = get_pos(flex_get_cluster_id());
    info.cluster_global_id      = flex_get_cluster_id();
    info.cluster_pos            = pos;

    //Spatz information
    info.spatz_num              = ARCH_SPATZ_ATTACED_CORES;
    uint32_t temp[ARCH_NUM_CORE_PER_CLUSTER] = ARCH_SPATZ_ATTACED_CHECK_LIST;
    for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i){info.spatz_check_list[i] = temp[i];}
    uint32_t sidt[ARCH_NUM_CORE_PER_CLUSTER] = ARCH_SPATZ_ATTACED_SID_LIST;
    for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i){info.spatz_sid_list[i] = sidt[i];}
    info.spatz_attached         = info.spatz_check_list[flex_get_core_id()];
    info.spatz_sid              = info.spatz_sid_list[flex_get_core_id()];

    //Tiling information
    info.d                      = info.head_dimemsion;
    info.Br                     = info.shape_y;
    info.Bc                     = info.shape_x;
    info.Tr                     = info.q_sequence_length / info.Br;
    info.Tc                     = info.kv_sequence_length / info.Bc;
#ifdef ATTN_FP16
    info.sqrt_dim               = info.d == 256? 0x4C00 : info.d == 128? 0x2DAB : info.d == 64? 0x4800 : asm_fp16_sqrt_fp32((float)info.d);
#endif
#ifdef ATTN_FP8
    info.sqrt_dim               = info.d == 256? 0x4C : info.d == 128? 0x49 : info.d == 64? 0x48 : asm_fp8_e5m2_sqrt_fp32((float)info.d);
#endif

    //L1 location information
    info.L1_Q                   = local(0);
    info.L1_KT                  = info.L1_Q     + info.Br * info.d  * DATA_TYPE_BYTE;
    info.L1_V                   = info.L1_KT    + info.d  * info.Bc * DATA_TYPE_BYTE;
    info.L1_O                   = info.L1_V     + info.Bc * info.d  * DATA_TYPE_BYTE;
    info.L1_A                   = info.L1_O     + info.Br * info.d  * DATA_TYPE_BYTE;
    info.DB_L1_Q                = info.L1_A     + info.Br * info.Bc * DATA_TYPE_BYTE;
    info.DB_L1_KT               = info.DB_L1_Q  + info.Br * info.d  * DATA_TYPE_BYTE;
    info.DB_L1_V                = info.DB_L1_KT + info.d  * info.Bc * DATA_TYPE_BYTE;
    info.DB_L1_O                = info.DB_L1_V  + info.Bc * info.d  * DATA_TYPE_BYTE;
    info.DB_L1_A                = info.DB_L1_O  + info.Br * info.d  * DATA_TYPE_BYTE;
    info.L1_m                   = info.DB_L1_A  + info.Br * info.Bc * DATA_TYPE_BYTE;
    info.L1_l                   = info.L1_m     + info.Br * 1       * DATA_TYPE_BYTE;
    info.L1_e                   = info.L1_l     + info.Br * 1       * DATA_TYPE_BYTE;
    info.L1_mr                  = info.L1_e     + info.Br * 1       * DATA_TYPE_BYTE;
    info.L1_lr                  = info.L1_mr    + info.Br * 1       * DATA_TYPE_BYTE;
    info.DB_L1_m                = info.L1_lr    + info.Br * 1       * DATA_TYPE_BYTE;
    info.DB_L1_l                = info.DB_L1_m  + info.Br * 1       * DATA_TYPE_BYTE;
    info.DB_L1_e                = info.DB_L1_l  + info.Br * 1       * DATA_TYPE_BYTE;
    info.DB_L1_mr               = info.DB_L1_e  + info.Br * 1       * DATA_TYPE_BYTE;
    info.DB_L1_lr               = info.DB_L1_mr + info.Br * 1       * DATA_TYPE_BYTE;

    //Usefull parameters for data layout address calculation
    info.L1_Q_size              = info.Br     * info.d    * DATA_TYPE_BYTE;
    info.L1_KT_size             = info.d      * info.Bc   * DATA_TYPE_BYTE;
    info.L1_V_size              = info.Bc     * info.d    * DATA_TYPE_BYTE;
    info.L1_O_size              = info.Br     * info.d    * DATA_TYPE_BYTE;
    info.L1_A_size              = info.Br     * info.Bc   * DATA_TYPE_BYTE;
    info.L1_m_size              = info.Br     * 1         * DATA_TYPE_BYTE;
    info.L1_l_size              = info.Br     * 1         * DATA_TYPE_BYTE;
    info.L1_e_size              = info.Br     * 1         * DATA_TYPE_BYTE;
    info.block_KTV_size         = info.d      * info.Bc   * DATA_TYPE_BYTE;
    info.block_QO_size          = info.Br     * info.d    * DATA_TYPE_BYTE;
    info.heads_KTV_size         = info.d      * info.kv_sequence_length * DATA_TYPE_BYTE;
    info.heads_QO_size          = info.d      * info.q_sequence_length  * DATA_TYPE_BYTE;

    //L1 location for spatz partition
    info.L1SP_O                 = info.L1_O     + info.spatz_sid * (info.L1_O_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_A                 = info.L1_A     + info.spatz_sid * (info.L1_A_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_m                 = info.L1_m     + info.spatz_sid * (info.L1_m_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_l                 = info.L1_l     + info.spatz_sid * (info.L1_l_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_e                 = info.L1_e     + info.spatz_sid * (info.L1_e_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_mr                = info.L1_mr    + info.spatz_sid * (info.L1_m_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_lr                = info.L1_lr    + info.spatz_sid * (info.L1_l_size / ARCH_SPATZ_ATTACED_CORES);

    info.DB_L1SP_O              = info.DB_L1_O  + info.spatz_sid * (info.L1_O_size / ARCH_SPATZ_ATTACED_CORES);
    info.DB_L1SP_A              = info.DB_L1_A  + info.spatz_sid * (info.L1_A_size / ARCH_SPATZ_ATTACED_CORES);
    info.DB_L1SP_m              = info.DB_L1_m  + info.spatz_sid * (info.L1_m_size / ARCH_SPATZ_ATTACED_CORES);
    info.DB_L1SP_l              = info.DB_L1_l  + info.spatz_sid * (info.L1_l_size / ARCH_SPATZ_ATTACED_CORES);
    info.DB_L1SP_e              = info.DB_L1_e  + info.spatz_sid * (info.L1_e_size / ARCH_SPATZ_ATTACED_CORES);
    info.DB_L1SP_mr             = info.DB_L1_mr + info.spatz_sid * (info.L1_m_size / ARCH_SPATZ_ATTACED_CORES);
    info.DB_L1SP_lr             = info.DB_L1_lr + info.spatz_sid * (info.L1_l_size / ARCH_SPATZ_ATTACED_CORES);

    //Workload information
    info.work_group_enable      = 1;
    info.work_group_head_num    = (info.batch_size * info.head_num) / ARCH_NUM_CLUSTER;
    if (info.work_group_head_num == 0){
        info.work_group_enable  = (info.cluster_global_id < (info.batch_size * info.head_num))? 1:0;
        info.work_group_head_num= 1;
    }
    info.work_group_head_start  = info.work_group_head_num * info.cluster_global_id;
    info.HBM_Q                  = Q_base_address + info.work_group_head_start * info.head_dimemsion * info.q_sequence_length  * DATA_TYPE_BYTE;
    info.HBM_K                  = K_base_address + info.work_group_head_start * info.head_dimemsion * info.kv_sequence_length * DATA_TYPE_BYTE;
    info.HBM_V                  = V_base_address + info.work_group_head_start * info.head_dimemsion * info.kv_sequence_length * DATA_TYPE_BYTE;
    info.HBM_O                  = O_base_address + info.work_group_head_start * info.head_dimemsion * info.q_sequence_length  * DATA_TYPE_BYTE;

    return info;
}

#endif
