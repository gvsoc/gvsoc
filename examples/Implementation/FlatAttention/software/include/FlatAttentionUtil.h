// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 6.Jan.2025

#ifndef _FLATATTENTION_UTIL_H_
#define _FLATATTENTION_UTIL_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_libfp16.h"
#include "flex_dump.h"

typedef struct FlatAttentionInfo
{
    //Attention General information
    uint32_t                sequence_length;
    uint32_t                head_dimemsion;
    uint32_t                head_num;
    uint32_t                batch_size;
    uint32_t                elem_size;

    //Flatten Settings
    uint32_t                flatten_scale_x;
    uint32_t                flatten_scale_y;
    uint32_t                flatten_shape_x;
    uint32_t                flatten_shape_y;
    uint32_t                flatten_slice_x;
    uint32_t                flatten_slice_y;

    //Group infomation
    GridSyncGroupInfo       group;

    //Cluster information
    FlexPosition            cluster_pos;
    uint32_t                cluster_global_id;
    uint32_t                cluster_in_group_id;
    uint32_t                cluster_in_group_id_x;
    uint32_t                cluster_in_group_id_y;

    //Flatten slice information
    uint32_t                slice_id_x;
    uint32_t                slice_id_y;
    uint32_t                slice_is_x_edge;
    uint32_t                slice_is_y_edge;

    //Tiling information
    uint32_t                d;
    uint32_t                Br;
    uint32_t                Bc;
    uint32_t                Tr;
    uint32_t                Tc;
    uint32_t                Br_s;
    uint32_t                Bc_s;

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
    uint32_t                work_group_head_num;
    uint32_t                work_group_head_start;
    uint64_t                HBM_Q;
    uint64_t                HBM_KT;
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
    uint32_t                slice_KTV_size;
    uint32_t                slice_QO_size;

}FlatAttentionInfo;

FlatAttentionInfo flat_attention_analyze(
    uint32_t                sequence_length,
    uint32_t                head_dimemsion,
    uint32_t                head_num,
    uint32_t                batch_size,
    uint32_t                elem_size,
    uint32_t                flatten_scale,
    uint32_t                flatten_shape){

    FlatAttentionInfo info;

    //Convert to full parameter scheme
    uint32_t flatten_scale_x    = flatten_scale;
    uint32_t flatten_scale_y    = flatten_scale;
    uint32_t flatten_shape_x    = flatten_shape;
    uint32_t flatten_shape_y    = flatten_shape;
    uint32_t flatten_slice_x    = flatten_scale;
    uint32_t flatten_slice_y    = flatten_scale;

    //Attention General information
    info.sequence_length        = sequence_length;
    info.head_dimemsion         = head_dimemsion;
    info.head_num               = head_num;
    info.batch_size             = batch_size;
    info.elem_size              = elem_size;

    //Flatten Settings
    info.flatten_scale_x        = flatten_scale_x;
    info.flatten_scale_y        = flatten_scale_y;
    info.flatten_shape_x        = flatten_shape_x;
    info.flatten_shape_y        = flatten_shape_y;
    info.flatten_slice_x        = flatten_slice_x;
    info.flatten_slice_y        = flatten_slice_y;

    //initialize group
    info.group                  = grid_sync_group_init(flatten_scale_x,flatten_scale_y);

    //Cluster information
    FlexPosition pos            = get_pos(flex_get_cluster_id());
    info.cluster_global_id      = flex_get_cluster_id();
    info.cluster_pos            = pos;
    info.cluster_in_group_id_x  = pos.x % info.group.grid_x_dim;
    info.cluster_in_group_id_y  = pos.y % info.group.grid_y_dim;
    info.cluster_in_group_id    = info.cluster_in_group_id_x + info.group.this_grid_cluster_num_x * info.cluster_in_group_id_y;

    //Flatten slice information
    info.slice_id_x             = info.cluster_in_group_id % info.flatten_slice_x;
    info.slice_id_y             = info.cluster_in_group_id / info.flatten_slice_x;
    info.slice_is_x_edge        = (info.slice_id_x == 0);
    info.slice_is_y_edge        = (info.slice_id_y == 0);

    //Tiling information
    info.d                      = info.head_dimemsion;
    info.Br                     = info.flatten_shape_y;
    info.Bc                     = info.flatten_shape_x;
    info.Tr                     = info.sequence_length / info.Br;
    info.Tc                     = info.sequence_length / info.Bc;
    info.Br_s                   = info.flatten_shape_y / info.flatten_slice_y;
    info.Bc_s                   = info.flatten_shape_x / info.flatten_slice_x;

    //L1 location information
    info.L1_Q                   = local(0);
    info.L1_KT                  = info.L1_Q     + info.Br_s * info.d    * info.elem_size;
    info.L1_V                   = info.L1_KT    + info.d    * info.Bc_s * info.elem_size;
    info.L1_O                   = info.L1_V     + info.Bc_s * info.d    * info.elem_size;
    info.L1_A                   = info.L1_O     + info.Br_s * info.d    * info.elem_size;
    info.DB_L1_Q                = info.L1_A     + info.Br_s * info.Bc_s * info.elem_size;
    info.DB_L1_KT               = info.DB_L1_Q  + info.Br_s * info.d    * info.elem_size;
    info.DB_L1_V                = info.DB_L1_KT + info.d    * info.Bc_s * info.elem_size;
    info.DB_L1_O                = info.DB_L1_V  + info.Bc_s * info.d    * info.elem_size;
    info.DB_L1_A                = info.DB_L1_O  + info.Br_s * info.d    * info.elem_size;
    info.L1_m                   = info.DB_L1_A  + info.Br_s * info.Bc_s * info.elem_size;
    info.L1_l                   = info.L1_m     + info.Br_s * 1         * info.elem_size;
    info.L1_e                   = info.L1_l     + info.Br_s * 1         * info.elem_size;
    info.L1_mr                  = info.L1_e     + info.Br_s * 1         * info.elem_size;
    info.L1_lr                  = info.L1_mr    + info.Br_s * 1         * info.elem_size;
    info.DB_L1_m                = info.L1_lr    + info.Br_s * 1         * info.elem_size;
    info.DB_L1_l                = info.DB_L1_m  + info.Br_s * 1         * info.elem_size;
    info.DB_L1_e                = info.DB_L1_l  + info.Br_s * 1         * info.elem_size;
    info.DB_L1_mr               = info.DB_L1_e  + info.Br_s * 1         * info.elem_size;
    info.DB_L1_lr               = info.DB_L1_mr + info.Br_s * 1         * info.elem_size;

    //Usefull parameters for data layout address calculation
    info.L1_Q_size              = info.Br_s * info.d    * info.elem_size;
    info.L1_KT_size             = info.d    * info.Bc_s * info.elem_size;
    info.L1_V_size              = info.Bc_s * info.d    * info.elem_size;
    info.L1_O_size              = info.Br_s * info.d    * info.elem_size;
    info.L1_A_size              = info.Br_s * info.Bc_s * info.elem_size;
    info.L1_m_size              = info.Br_s * 1         * info.elem_size;
    info.L1_l_size              = info.Br_s * 1         * info.elem_size;
    info.L1_e_size              = info.Br_s * 1         * info.elem_size;
    info.slice_KTV_size         = info.L1_KT_size;
    info.slice_QO_size          = info.L1_Q_size;

    //Workload information
    info.work_group_head_num    = (info.batch_size * info.head_num) / (info.group.grid_x_num * info.group.grid_y_num);
    info.work_group_head_start  = info.work_group_head_num * info.group.this_grid_id;
    info.HBM_Q                  = TILED_HBM_Q(pos,  info.Tr, info.L1_Q_size,  info.work_group_head_num, info.group.this_grid_id_x, info.group.this_grid_id_y, info.group.grid_x_num, info.group.grid_y_num);
    info.HBM_KT                 = TILED_HBM_KT(pos, info.Tc, info.L1_KT_size, info.work_group_head_num, info.group.this_grid_id_x, info.group.this_grid_id_y, info.group.grid_x_num, info.group.grid_y_num);
    info.HBM_V                  = TILED_HBM_V(pos,  info.Tc, info.L1_V_size,  info.work_group_head_num, info.group.this_grid_id_x, info.group.this_grid_id_y, info.group.grid_x_num, info.group.grid_y_num);
    info.HBM_O                  = TILED_HBM_O(pos,  info.Tr, info.L1_O_size,  info.work_group_head_num, info.group.this_grid_id_x, info.group.this_grid_id_y, info.group.grid_x_num, info.group.grid_y_num);

    return info;
}

void flat_attention_print_info(FlatAttentionInfo* info) {
    printf("    Attention General information\n");
    printf("        Sequence Length:            %u\n", info->sequence_length);
    printf("        Head Dimension:             %u\n", info->head_dimemsion);
    printf("        Head Num:                   %u\n", info->head_num);
    printf("        Batch Size:                 %u\n", info->batch_size);
    printf("        Elem Size:                  %u\n", info->elem_size);
    printf("    Flatten Settings\n");
    printf("        Flatten Scale X:            %u\n", info->flatten_scale_x);
    printf("        Flatten Scale Y:            %u\n", info->flatten_scale_y);
    printf("        Flatten Shape X:            %u\n", info->flatten_shape_x);
    printf("        Flatten Shape Y:            %u\n", info->flatten_shape_y);
    printf("        Flatten Slice X:            %u\n", info->flatten_slice_x);
    printf("        Flatten Slice Y:            %u\n", info->flatten_slice_y);
    printf("    Cluster information\n");
    printf("        Cluster Global   ID:        %u\n", info->cluster_global_id);
    printf("        Cluster Group    ID:        %u\n", info->group.this_grid_id);
    printf("        Cluster In Group ID:        %u\n", info->cluster_in_group_id);
    printf("        Cluster In Group ID X:      %u\n", info->cluster_in_group_id_x);
    printf("        Cluster In Group ID Y:      %u\n", info->cluster_in_group_id_y);
    printf("    Flatten slice information\n");
    printf("        Slice ID X:                 %u\n", info->slice_id_x);
    printf("        Slice ID Y:                 %u\n", info->slice_id_y);
    printf("        Slice is X Edge:            %u\n", info->slice_is_x_edge);
    printf("        Slice is Y Edge:            %u\n", info->slice_is_y_edge);
    printf("    Tiling information\n");
    printf("        D:                          %u\n", info->d);
    printf("        Br:                         %u\n", info->Br);
    printf("        Bc:                         %u\n", info->Bc);
    printf("        Tr:                         %u\n", info->Tr);
    printf("        Tc:                         %u\n", info->Tc);
    printf("        Br_s:                       %u\n", info->Br_s);
    printf("        Bc_s:                       %u\n", info->Bc_s);
    printf("    L1 location information\n");
    printf("        L1_Q:                       0x%0x\n", info->L1_Q);
    printf("        L1_KT:                      0x%0x\n", info->L1_KT);
    printf("        L1_V:                       0x%0x\n", info->L1_V);
    printf("        L1_O:                       0x%0x\n", info->L1_O);
    printf("        L1_A:                       0x%0x\n", info->L1_A);
    printf("        DB_L1_Q:                    0x%0x\n", info->DB_L1_Q);
    printf("        DB_L1_KT:                   0x%0x\n", info->DB_L1_KT);
    printf("        DB_L1_V:                    0x%0x\n", info->DB_L1_V);
    printf("        DB_L1_O:                    0x%0x\n", info->DB_L1_O);
    printf("        DB_L1_A:                    0x%0x\n", info->DB_L1_A);
    printf("        L1_m:                       0x%0x\n", info->L1_m);
    printf("        L1_l:                       0x%0x\n", info->L1_l);
    printf("        L1_e:                       0x%0x\n", info->L1_e);
    printf("        L1_mr:                      0x%0x\n", info->L1_mr);
    printf("        L1_lr:                      0x%0x\n", info->L1_lr);
    printf("        DB_L1_m:                    0x%0x\n", info->DB_L1_m);
    printf("        DB_L1_l:                    0x%0x\n", info->DB_L1_l);
    printf("        DB_L1_e:                    0x%0x\n", info->DB_L1_e);
    printf("        DB_L1_mr:                   0x%0x\n", info->DB_L1_mr);
    printf("        DB_L1_lr:                   0x%0x\n", info->DB_L1_lr);
    printf("    Workload information\n");
    printf("        Work Group Head Num:        %u\n", info->work_group_head_num);
    printf("        Work Group Head Start:      %u\n", info->work_group_head_start);
    uint32_t H,L;
    H = info->HBM_Q >> 32;
    L = info->HBM_Q;
    printf("        HBM_Q:                      0x%08x_%08x\n", H, L);
    H = info->HBM_KT >> 32;
    L = info->HBM_KT;
    printf("        HBM_KT:                     0x%08x_%08x\n", H, L);
    H = info->HBM_V >> 32;
    L = info->HBM_V;
    printf("        HBM_V:                      0x%08x_%08x\n", H, L);
    H = info->HBM_O >> 32;
    L = info->HBM_O;
    printf("        HBM_O:                      0x%08x_%08x\n", H, L);
}


inline void flat_attention_broadcast_rowwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_in_group_id_x == 0)
    {
        flex_dma_async_1d_broadcast(remote_xy(info->group.this_grid_right_most,info->cluster_pos.y,offset), local(offset), size);
        flex_dma_async_wait_all();
    }
}

inline void flat_attention_broadcast_colwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_in_group_id_y == 0)
    {
        flex_dma_async_1d_broadcast(remote_xy(info->cluster_pos.x,info->group.this_grid_top_most,offset), local(offset), size);
        flex_dma_async_wait_all();
    }
}

inline void flat_attention_redmax_rowwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_pos.x == info->group.this_grid_right_most)
    {
        flex_dma_async_1d_reduction(remote_xy(info->group.this_grid_left_most,info->cluster_pos.y,offset), local(offset), size, COLLECTIVE_REDMAX_FP_16);
        flex_dma_async_wait_all();
    }
}

inline void flat_attention_redmax_colwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_pos.y == info->group.this_grid_top_most)
    {
        flex_dma_async_1d_reduction(remote_xy(info->cluster_pos.x, info->group.this_grid_bottom_most, offset), local(offset), size, COLLECTIVE_REDMAX_FP_16);
        flex_dma_async_wait_all();
    }
}

inline void flat_attention_redsum_rowwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_pos.x == info->group.this_grid_right_most)
    {
        flex_dma_async_1d_reduction(remote_xy(info->group.this_grid_left_most,info->cluster_pos.y,offset), local(offset), size, COLLECTIVE_REDADD_FP_16);
        flex_dma_async_wait_all();
    }
}

inline void flat_attention_redsum_colwise(FlatAttentionInfo* info, uint32_t offset, uint32_t size){
    if (flex_is_dm_core() && info->cluster_pos.y == info->group.this_grid_top_most)
    {
        flex_dma_async_1d_reduction(remote_xy(info->cluster_pos.x, info->group.this_grid_bottom_most, offset), local(offset), size, COLLECTIVE_REDADD_FP_16);
        flex_dma_async_wait_all();
    }
}


#define REPEAT_1(x) x
#define REPEAT_2(x) REPEAT_1(x) x
#define REPEAT_4(x) REPEAT_2(x) REPEAT_2(x)
#define REPEAT_8(x) REPEAT_4(x) REPEAT_4(x)
#define REPEAT_16(x) REPEAT_8(x) REPEAT_8(x)
#define REPEAT_32(x) REPEAT_16(x) REPEAT_16(x)
#define REPEAT_64(x) REPEAT_32(x) REPEAT_32(x)
#define REPEAT_128(x) REPEAT_64(x) REPEAT_64(x)


inline void flat_attention_vector_rowmax_MV_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src, uint32_t V_dst){
    uint32_t vl;
    uint16_t sqrt_dim = 0x2DAB;
    asm volatile("fld fa5, (%0)" ::"r"(&sqrt_dim));
    asm volatile("li a7, 0":::"a7");
    //load V_src
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vle16.v v0,  (%0)" ::"r"(V_src));
    //Scale and Redmax for each row of M_src
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_cols));\
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("vle16.v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfmul.vf v8, v8, fa5");\
            asm volatile("vse16.v v8,  (%0)" ::"r"(M_src));\
            asm volatile(".word %0\n"::"i"(0x1e88b057):"a7") /*vfredmax.vx v0, a7, v8 --> Customized Instruction: v0[a7] = v0[a7] + RedMax(v8)*/; \
            asm volatile("addi a7, a7, 1":::"a7"); \
            M_src += num_cols * 2;\
        )
    }
    //Store back max value
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vse16.v v0,  (%0)" ::"r"(V_dst));
}


inline void flat_attention_vector_EXP_MV(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_cols));
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("fld fa5, (%0)" ::"r"(V_src));\
            asm volatile("vle16.v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfsub.vf v8, v8, fa5");\
            asm volatile(".word %0\n"::"i"(0x32041857)) /*vfexp.vv v16, v8*/; \
            asm volatile("vse16.v v16,  (%0)" ::"r"(M_src));\
            M_src += num_cols * 2;\
            V_src += 2;\
        )
    }
}

inline void flat_attention_vector_EXP_VV_V(uint32_t vector_length, uint32_t Vr_src, uint32_t V_src, uint32_t V_dst){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(vector_length));
    asm volatile("vle16.v v0,  (%0)" ::"r"(Vr_src));
    asm volatile("vle16.v v8,  (%0)" ::"r"(V_src));
    asm volatile("vfsub.vv v8, v0, v8");
    asm volatile(".word %0\n"::"i"(0x32041857)) /*vfexp.vv v16, v8*/; \
    asm volatile("vse16.v v16,  (%0)" ::"r"(V_dst));
}


inline void flat_attention_vector_rowsum_M_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_dst){
    uint32_t vl;
    asm volatile("li a7, 0":::"a7");
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vmv.v.x v0, %0" ::"r"(0));
    //Redsum for each row of M_src
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_cols));\
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("vle16.v v8,  (%0)" ::"r"(M_src));\
            asm volatile(".word %0\n"::"i"(0x0688b057):"a7") /*vfredsum.vx v0, a7, v8 --> Customized Instruction: v0[a7] = v0[a7] + RedSum(v8)*/; \
            asm volatile("addi a7, a7, 1":::"a7"); \
            M_src += num_cols * 2;\
        )
    }
    //Store back max value
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vse16.v v0,  (%0)" ::"r"(V_dst));
}

//l = e-*-lr + l
inline void flat_attention_vector_update_l(uint32_t vector_length, uint32_t lr, uint32_t e, uint32_t l){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(vector_length));
    asm volatile("vle16.v v0,  (%0)" ::"r"(lr));
    asm volatile("vle16.v v8,  (%0)" ::"r"(e));
    asm volatile("vle16.v v16,  (%0)" ::"r"(l));
    asm volatile("vfmul.vv v8, v8, v0");
    asm volatile("vfadd.vv v16, v16, v8");
    asm volatile("vse16.v v16,  (%0)" ::"r"(l));
}

void flat_attention_vector_M_div_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(num_cols));
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("fld fa5, (%0)" ::"r"(V_src));\
            asm volatile("vle16.v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfdiv.vf v8, v8, fa5");\
            asm volatile("vse16.v v8,  (%0)" ::"r"(M_src));\
            M_src += num_cols * 2;\
            V_src += 2;\
        )
    }
}



#endif