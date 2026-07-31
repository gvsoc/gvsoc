#ifndef _MLA_DECODE_MHA_H_
#define _MLA_DECODE_MHA_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_transpose_engine.h"
#include "flex_libfp16.h"
#include "flex_libfp8.h"
#include "tmla.h"

#define NOPE_SPLIT 4
inline void flat_attention_vector_rowmax_MV_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src, uint32_t V_dst, tmla_data_t sqrt_dim);
inline void flat_attention_vector_EXP_MV(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src);
inline void flat_attention_vector_EXP_VV_V(uint32_t vector_length, uint32_t Vr_src, uint32_t V_src, uint32_t V_dst);
inline void flat_attention_vector_rowsum_M_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_dst);
inline void flat_attention_vector_update_l(uint32_t vector_length, uint32_t lr, uint32_t e, uint32_t l);
void flat_attention_vector_M_div_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src);
void flat_attention_vector_M_mul_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src);


typedef struct FlatMLAInfo
{
    //General information
    uint64_t                Q_NOPE_address;
    uint64_t                Q_ROPE_address;
    uint64_t                C_NOPE_address;
    uint64_t                C_ROPE_address;
    uint64_t                O_address;
    uint32_t                kv_sequence_length;
    uint32_t                q_sequence_length;
    uint32_t                speculative_length;
    uint32_t                nope_head_dim;
    uint32_t                rope_head_dim;
    uint32_t                total_head_dim;
    uint32_t                num_head;
    uint32_t                batch_size;

    //Group infomation
    uint32_t                flatten_scale_x;
    uint32_t                flatten_scale_y;
    uint32_t                flatten_shape_x;
    uint32_t                flatten_shape_y;
    GridSyncGroupInfo       group;

    //Cluster information
    FlexPosition            cluster_pos;
    uint32_t                cluster_global_id;
    uint32_t                cluster_in_group_id;
    uint32_t                cluster_in_group_id_x;
    uint32_t                cluster_in_group_id_y;
    uint32_t                cluster_for_rowwise;
    uint32_t                cluster_for_colwise;

     //Spatz Information
    uint32_t                spatz_num;
    uint32_t                spatz_check_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t                spatz_sid_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t                spatz_attached;
    uint32_t                spatz_sid;

    //Tiling information
    uint32_t                d;
    uint32_t                d_rope;
    uint32_t                R;
    uint32_t                C;
    uint32_t                Br;
    uint32_t                Bc;
    uint32_t                Tr;
    uint32_t                Tc;
    uint32_t                Br_s;
    uint32_t                Bc_s;
    tmla_data_t             sqrt_dim;

    //L1 location information
    uint32_t                L1_Q1;
    uint32_t                L1_K1;
    uint32_t                L1_V1;
    uint32_t                L1_O1;
    uint32_t                L1_Q2;
    uint32_t                L1_K2;
    uint32_t                L1_V2;
    uint32_t                L1_O2;
    uint32_t                L1_S1;
    uint32_t                L1_S2;
    uint32_t                L1_O3;
    uint32_t                L1_O4;
    uint32_t                L1_m;
    uint32_t                L1_l;
    uint32_t                L1_e;
    uint32_t                L1_mr;
    uint32_t                L1_lr;

    //L1 location for spatz partition
    uint32_t                L1SP_O1;
    uint32_t                L1SP_O2;
    uint32_t                L1SP_O3;
    uint32_t                L1SP_O4;
    uint32_t                L1SP_S1;
    uint32_t                L1SP_m;
    uint32_t                L1SP_l;
    uint32_t                L1SP_e;
    uint32_t                L1SP_mr;
    uint32_t                L1SP_lr;

    //Usefull parameters for address calculation
    uint32_t                L1_Q_size;
    uint32_t                L1_K_size;
    uint32_t                L1_V_size;
    uint32_t                L1_O_size;
    uint32_t                L1_A_size;
    uint32_t                L1_m_size;
    uint32_t                L1_l_size;

    //Usefull informations for HBM address calculation
    uint32_t                slice_QN_size;
    uint32_t                slice_CN_size;
    uint32_t                slice_QR_size;
    uint32_t                slice_CR_size;
    uint32_t                slice_O_size;
    uint32_t                block_QN_size;
    uint32_t                block_CN_size;
    uint32_t                block_QR_size;
    uint32_t                block_CR_size;
    uint32_t                block_O_size;
    uint32_t                batch_QN_size;
    uint32_t                batch_CN_size;
    uint32_t                batch_QR_size;
    uint32_t                batch_CR_size;
    uint32_t                batch_O_size;

    //Workload information
    uint32_t                work_group_enable;
    uint32_t                work_group_batch;
    uint32_t                work_group_batch_start;
    uint64_t                HBM_QN;
    uint64_t                HBM_QR;
    uint64_t                HBM_CN;
    uint64_t                HBM_CR;
    uint64_t                HBM_O;


}FlatMLAInfo;

FlatMLAInfo FlatMLA_analyze(
    uint64_t Q_NOPE_address,
    uint64_t Q_ROPE_address,
    uint64_t C_NOPE_address,
    uint64_t C_ROPE_address,
    uint64_t O_address,
    uint32_t kv_sequence_length,
    uint32_t q_sequence_length,
    uint32_t speculative_length,
    uint32_t nope_head_dim,
    uint32_t rope_head_dim,
    uint32_t num_head,
    uint32_t batch_size,
    uint32_t flatten_scale_x,
    uint32_t flatten_scale_y,
    uint32_t flatten_shape_x,
    uint32_t flatten_shape_y){

    FlatMLAInfo info;

    //General information
    info.Q_NOPE_address             = Q_NOPE_address;
    info.Q_ROPE_address             = Q_ROPE_address;
    info.C_NOPE_address             = C_NOPE_address;
    info.C_ROPE_address             = C_ROPE_address;
    info.O_address                  = O_address;
    info.kv_sequence_length         = kv_sequence_length;
    info.q_sequence_length          = q_sequence_length;
    info.speculative_length         = speculative_length;
    info.nope_head_dim              = nope_head_dim;
    info.rope_head_dim              = rope_head_dim;
    info.total_head_dim             = info.nope_head_dim + info.rope_head_dim;
    info.num_head                   = num_head;
    info.batch_size                 = batch_size;

    //Group infomation
    //Flatten Settings
    info.flatten_scale_x            = flatten_scale_x;
    info.flatten_scale_y            = flatten_scale_y;
    info.flatten_shape_x            = flatten_shape_x;
    info.flatten_shape_y            = flatten_shape_y;
    info.group                      = grid_sync_group_init(info.flatten_scale_x,info.flatten_scale_y);

    //Cluster information
    FlexPosition pos                = get_pos(flex_get_cluster_id());
    info.cluster_global_id          = flex_get_cluster_id();
    info.cluster_pos                = pos;
    info.cluster_in_group_id_x      = pos.x % info.group.grid_x_dim;
    info.cluster_in_group_id_y      = pos.y % info.group.grid_y_dim;
    info.cluster_in_group_id        = info.cluster_in_group_id_x + info.group.this_grid_cluster_num_x * info.cluster_in_group_id_y;
    info.cluster_for_rowwise        = ((info.cluster_in_group_id_x % info.group.grid_y_dim) == (info.cluster_in_group_id_y % info.group.grid_x_dim) && (info.cluster_in_group_id_x == (pos.y % info.group.grid_x_dim)))? 1 : 0;
    info.cluster_for_colwise        = ((info.cluster_in_group_id_x % info.group.grid_y_dim) == (info.cluster_in_group_id_y % info.group.grid_x_dim) && (info.cluster_in_group_id_y == (pos.x % info.group.grid_y_dim)))? 1 : 0;

    //Spatz information
    info.spatz_num                  = ARCH_SPATZ_ATTACED_CORES;
    uint32_t temp[ARCH_NUM_CORE_PER_CLUSTER] = ARCH_SPATZ_ATTACED_CHECK_LIST;
    for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i){info.spatz_check_list[i] = temp[i];}
    uint32_t sidt[ARCH_NUM_CORE_PER_CLUSTER] = ARCH_SPATZ_ATTACED_SID_LIST;
    for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i){info.spatz_sid_list[i] = sidt[i];}
    info.spatz_attached             = info.spatz_check_list[flex_get_core_id()];
    info.spatz_sid                  = info.spatz_sid_list[flex_get_core_id()];

    //Tiling information
    info.d                          = info.nope_head_dim / NOPE_SPLIT;
    info.d_rope                     = info.rope_head_dim;
    info.R                          = info.q_sequence_length * info.speculative_length * info.num_head;
    info.C                          = info.kv_sequence_length;
    info.Br                         = info.flatten_shape_y;
    info.Bc                         = info.flatten_shape_x;
    info.Tr                         = (info.R + info.Br - 1) / info.Br;
    info.Tc                         = (info.C + info.Bc - 1) / info.Bc;
    info.Br_s                       = info.flatten_shape_y / info.flatten_scale_y;
    info.Bc_s                       = info.flatten_shape_x / info.flatten_scale_x;
#ifdef TMLA_FP16
    info.sqrt_dim                   = info.total_head_dim == 256? 0x4C00 : info.total_head_dim == 128? 0x2DAB : info.total_head_dim == 64? 0x4800 : info.total_head_dim == 576? 0x4E00 : asm_fp16_sqrt_fp32((float)info.total_head_dim);
#endif
#ifdef TMLA_FP8
    info.sqrt_dim                   = info.total_head_dim == 256? 0x4C : info.total_head_dim == 128? 0x49 : info.total_head_dim == 64? 0x48 : info.total_head_dim == 576? 0x4E : asm_fp8_e5m2_sqrt_fp32((float)info.total_head_dim);
#endif

    //L1 location information
    info.L1_Q1                      = local(0);
    info.L1_K1                      = info.L1_Q1    + info.Br_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_V1                      = info.L1_K1    + info.Bc_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_O1                      = info.L1_V1    + info.Bc_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_Q2                      = info.L1_O1    + info.Br_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_K2                      = info.L1_Q2    + info.Br_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_V2                      = info.L1_K2    + info.Bc_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_O2                      = info.L1_V2    + info.Bc_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_S1                      = info.L1_O2    + info.Br_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_S2                      = info.L1_S1    + info.Br_s     * info.Bc_s * DATA_TYPE_BYTE;
    info.L1_O3                      = info.L1_S1    + info.Br_s     * info.Bc_s * DATA_TYPE_BYTE;
    info.L1_O4                      = info.L1_O3    + info.Br_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_m                       = info.L1_O4    + info.Br_s     * info.d    * DATA_TYPE_BYTE;
    info.L1_l                       = info.L1_m     + info.Br_s     * 1         * DATA_TYPE_BYTE;
    info.L1_e                       = info.L1_l     + info.Br_s     * 1         * DATA_TYPE_BYTE;
    info.L1_mr                      = info.L1_e     + info.Br_s     * 1         * DATA_TYPE_BYTE;
    info.L1_lr                      = info.L1_mr    + info.Br_s     * 1         * DATA_TYPE_BYTE;


    //Usefull parameters for data layout address calculation
    info.L1_Q_size                  = info.Br_s     * info.d        * DATA_TYPE_BYTE;
    info.L1_K_size                  = info.Bc_s     * info.d        * DATA_TYPE_BYTE;
    info.L1_V_size                  = info.Bc_s     * info.d        * DATA_TYPE_BYTE;
    info.L1_O_size                  = info.Br_s     * info.d        * DATA_TYPE_BYTE;
    info.L1_A_size                  = info.Br_s     * info.Bc_s     * DATA_TYPE_BYTE;
    info.L1_m_size                  = info.Br_s     * 1             * DATA_TYPE_BYTE;
    info.L1_l_size                  = info.Br_s     * 1             * DATA_TYPE_BYTE;

    //L1 location for spatz partition
    info.L1SP_O1                    = info.L1_O1    + info.spatz_sid * (info.L1_O_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_O2                    = info.L1_O2    + info.spatz_sid * (info.L1_O_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_O3                    = info.L1_O3    + info.spatz_sid * (info.L1_O_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_O4                    = info.L1_O4    + info.spatz_sid * (info.L1_O_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_S1                    = info.L1_S1    + info.spatz_sid * (info.L1_A_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_m                     = info.L1_m     + info.spatz_sid * (info.L1_m_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_l                     = info.L1_l     + info.spatz_sid * (info.L1_l_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_e                     = info.L1_e     + info.spatz_sid * (info.L1_m_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_mr                    = info.L1_mr    + info.spatz_sid * (info.L1_m_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_lr                    = info.L1_lr    + info.spatz_sid * (info.L1_l_size / ARCH_SPATZ_ATTACED_CORES);

    //Usefull informations for HBM address calculation
    info.slice_QN_size              = info.Br_s     * info.nope_head_dim    * DATA_TYPE_BYTE;
    info.slice_CN_size              = info.Bc_s     * info.nope_head_dim    * DATA_TYPE_BYTE;
    info.slice_QR_size              = info.Br_s     * info.rope_head_dim    * DATA_TYPE_BYTE;
    info.slice_CR_size              = info.Bc_s     * info.rope_head_dim    * DATA_TYPE_BYTE;
    info.slice_O_size               = info.Br_s     * info.nope_head_dim    * DATA_TYPE_BYTE;
    info.block_QN_size              = info.Br       * info.nope_head_dim    * DATA_TYPE_BYTE;
    info.block_CN_size              = info.Bc       * info.nope_head_dim    * DATA_TYPE_BYTE;
    info.block_QR_size              = info.Br       * info.rope_head_dim    * DATA_TYPE_BYTE;
    info.block_CR_size              = info.Bc       * info.rope_head_dim    * DATA_TYPE_BYTE;
    info.block_O_size               = info.Br       * info.nope_head_dim    * DATA_TYPE_BYTE;
    info.batch_QN_size              = info.R        * info.nope_head_dim    * DATA_TYPE_BYTE;
    info.batch_CN_size              = info.C        * info.nope_head_dim    * DATA_TYPE_BYTE;
    info.batch_QR_size              = info.R        * info.rope_head_dim    * DATA_TYPE_BYTE;
    info.batch_CR_size              = info.C        * info.rope_head_dim    * DATA_TYPE_BYTE;
    info.batch_O_size               = info.R        * info.nope_head_dim    * DATA_TYPE_BYTE;

    //Workload information
    info.work_group_enable          = 1;
    info.work_group_batch           = info.batch_size / (info.group.grid_x_num * info.group.grid_y_num);
    if (info.work_group_batch == 0){
        info.work_group_enable      = (info.group.this_grid_id < info.batch_size)? 1:0;
        info.work_group_batch       = 1;
    }
    info.work_group_batch_start     = info.work_group_batch * info.group.this_grid_id;
    info.HBM_QN                     = Q_NOPE_address    + info.work_group_batch_start * info.batch_QN_size  + info.cluster_in_group_id_y * info.slice_QN_size;
    info.HBM_QR                     = Q_ROPE_address    + info.work_group_batch_start * info.batch_QR_size  + info.cluster_in_group_id_y * info.slice_QR_size;
    info.HBM_CN                     = C_NOPE_address    + info.work_group_batch_start * info.batch_CN_size  + info.cluster_in_group_id_x * info.slice_CN_size;
    info.HBM_CR                     = C_ROPE_address    + info.work_group_batch_start * info.batch_CR_size  + info.cluster_in_group_id_x * info.slice_CR_size;
    info.HBM_O                      = O_address         + info.work_group_batch_start * info.batch_O_size   + info.cluster_in_group_id_y * info.slice_O_size;

    return info;
}


void FlatMLA_run(FlatMLAInfo * info){
    uint32_t store_enable = 0;
    uint32_t store_b, store_r;
    for (int b = 0; b < info->work_group_batch; ++b)
    {
        for (int r = 0; r < info->Tr; ++r)
        {
            /********************************************************************
             *  0. initialization intermediate variables                        *
             *     -- Reset L1_m, L1_l                                          *
             ********************************************************************/
            if (flex_is_dm_core())
            {
                flex_dma_async_1d(info->L1_m,zomem(0),info->L1_m_size);
                flex_dma_async_1d(info->L1_l,zomem(0),info->L1_l_size);
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
            }

            for (int c = 0; c < info->Tc; ++c)
            {
                /********************************************************************
                 *  1. initialization DMA Actions                                   *
                 *     -- m => mr, l => lr                                          *
                 *     -- load QN_0, CN_0,                                          *
                 *     -- multicast QN_0, transpose & multicast CN_0                *
                 ********************************************************************/
                if (flex_is_dm_core())
                {
                    flex_dma_async_1d(info->L1_mr,info->L1_m,info->L1_m_size);
                    flex_dma_async_1d(info->L1_lr,info->L1_l,info->L1_l_size);
                    flex_dma_async_1d(info->L1_S1,zomem(0),info->L1_A_size);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->HBM_QN + b * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->HBM_CN + b * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K1,info->L1_K1,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }






                /********************************************************************
                 *  2. Q * KT Phase                                                 *
                 *     -- wait redmule -> intialize L1_S1 --> intra sync            *
                 *     -- load      QN_1, CN_1,                                     *
                 *     -- multicast QN_1, transpose & multicast CN_1                *
                 *     -- Compute   QN_0, CN_0 --> L1_S1                            *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->HBM_QN + b * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->HBM_CN + b * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K2,info->L1_K2,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }






                /********************************************************************
                 *  3. Q * KT Phase                                                 *
                 *     -- load      QN_2, CN_2,                                     *
                 *     -- multicast QN_2, transpose & multicast CN_2                *
                 *     -- Compute   QN_1, CN_1 --> L1_S1                            *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->HBM_QN + b * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->HBM_CN + b * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K1,info->L1_K1,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }





                /********************************************************************
                 *  4. Q * KT Phase                                                 *
                 *     -- load      QN_3, CN_3,                                     *
                 *     -- multicast QN_3, transpose & multicast CN_3                *
                 *     -- Compute   QN_2, CN_2 --> L1_S1                            *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->HBM_QN + b * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->HBM_CN + b * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K2,info->L1_K2,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }






                /********************************************************************
                 *  5. Q * KT Phase                                                 *
                 *     -- load      QR, CR,                                         *
                 *     -- multicast QR, transpose & multicast CR                    *
                 *     -- Compute   QN_3, CN_3 --> L1_S1                            *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->HBM_QR + b * info->batch_QR_size/*head offset*/ + r * info->block_QR_size/*i block offset*/,/*source*/
                            info->d_rope * DATA_TYPE_BYTE, /*transfer size*/
                            info->d_rope * DATA_TYPE_BYTE, /*destination stride*/
                            info->d_rope * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->HBM_CR + b * info->batch_CR_size/*head offset*/ + c * info->block_CR_size/*i block offset*/,/*source*/
                            info->d_rope * DATA_TYPE_BYTE, /*transfer size*/
                            info->d_rope * DATA_TYPE_BYTE, /*destination stride*/
                            info->d_rope * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d_rope,info->L1_K1,info->L1_K1,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }






                /********************************************************************
                 *  6. Q * KT Phase                                                 *
                 *     -- (if c == 0 && r !=0 ) store & (c==0)rest L1_O1            *
                 *     -- load      CN_0 --> L1_V1,                                 *
                 *     -- multicast CN_0                                            *
                 *     -- Compute   QR, CR --> L1_S1                                *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d_rope, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        if (c == 0)
                        {
                            if (store_enable)
                            {
                                //Reduction O
                                flex_dma_async_reduction(
                                    info->L1_O1/*dst_offset*/,
                                    info->L1_O1/*src_offset*/,
                                    info->L1_O_size/*transfer_size*/,
                                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                                    info->group.wakeup_row_mask/*row_mask*/,
                                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //Store O
                                flex_dma_async_2d(
                                    info->HBM_O + store_b * info->batch_O_size/*head offset*/ + store_r * info->block_O_size/*i block offset*/ + 0 * info->d * DATA_TYPE_BYTE, /*destination*/
                                    info->L1_O1, /*source*/
                                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                                    info->d * DATA_TYPE_BYTE, /*source stride*/
                                    info->Br_s /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                            }
                            flex_dma_async_1d(info->L1_O1,zomem(0),info->L1_O_size);
                            flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        }
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->HBM_CN + b * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }






                /********************************************************************
                 *  7. SoftMax Phase                                                *
                 *     -- m = max(mr, rowmax(L1_S1))                                *
                 *     -- Rowmax reducetion between clusters and multicast back     *
                 *     -- p = exp(L1_S1 - m)                                        *
                 *     -- l = rowsum(p)                                             *
                 *     -- Rowsum reducetion between clusters and multicast back     *
                 *     -- if(c!= 0) e = exp(mr - m); l = e-*-lr + l; O = O*e        *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (info->spatz_attached)
                {
                    // m = max(mr, rowmax(Att))
                    flat_attention_vector_rowmax_MV_V(
                        info->Bc_s,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_S1,
                        info->L1SP_mr,
                        info->L1SP_m,
                        info->sqrt_dim
                        );
                }
                if (info->flatten_scale_x > 1) {
                    grid_sync_group_barrier_xy(&(info->group));
                    if (flex_is_dm_core())
                    {
                        //Global maxima
                        flex_dma_async_reduction(
                            info->L1_m/*dst_offset*/,
                            info->L1_m/*src_offset*/,
                            info->L1_m_size/*transfer_size*/,
                            COLLECTIVE_REDMAX_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_broadcast(
                            info->L1_m/*dst_offset*/,
                            info->L1_m/*src_offset*/,
                            info->L1_m_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (info->spatz_attached)
                {
                    //p = exp(att - m)
                    flat_attention_vector_EXP_MV(
                        info->Bc_s,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_S1,
                        info->L1SP_m
                        );

                    //l = rowsum(p)
                    flat_attention_vector_rowsum_M_V(
                        info->Bc_s,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_S1,
                        info->L1SP_l
                        );
                }
                if (info->flatten_scale_x > 1) {
                    grid_sync_group_barrier_xy(&(info->group));
                    if (flex_is_dm_core())
                    {
                        //Global redsum
                        flex_dma_async_reduction(
                            info->L1_l/*dst_offset*/,
                            info->L1_l/*src_offset*/,
                            info->L1_l_size/*transfer_size*/,
                            COLLECTIVE_REDSUM_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_broadcast(
                            info->L1_l/*dst_offset*/,
                            info->L1_l/*src_offset*/,
                            info->L1_l_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (info->spatz_attached && c != 0)
                {
                    //Compute e = exp(mr - m)
                    flat_attention_vector_EXP_VV_V(
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_mr,
                        info->L1SP_m,
                        info->L1SP_e);

                    //Compute l = e-*-lr + l
                    flat_attention_vector_update_l(
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_lr,
                        info->L1SP_e,
                        info->L1SP_l);

                    //Compute O = O*e
                    flat_attention_vector_M_mul_V(
                        info->d,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O1,
                        info->L1SP_e);
                }






                /********************************************************************
                 *  8. S * V Phase                                                  *
                 *     -- (if c == 0 && r !=0 ) store & (c==0)rest L1_O2            *
                 *     -- load      CN_1 --> L1_V2,                                 *
                 *     -- multicast CN_1                                            *
                 *     -- Compute   L1_S1, CN_0 --> L1_O1                           *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->Bc_s, info->d);
                    flex_redmule_trigger(info->L1_S1, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                }
                if (info->spatz_attached && c != 0)
                {
                    //Compute O = O*e
                    flat_attention_vector_M_mul_V(
                        info->d,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O2,
                        info->L1SP_e);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        if (c == 0)
                        {
                            if (store_enable)
                            {
                                //Reduction O
                                flex_dma_async_reduction(
                                    info->L1_O2/*dst_offset*/,
                                    info->L1_O2/*src_offset*/,
                                    info->L1_O_size/*transfer_size*/,
                                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                                    info->group.wakeup_row_mask/*row_mask*/,
                                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //Store O
                                flex_dma_async_2d(
                                    info->HBM_O + store_b * info->batch_O_size/*head offset*/ + store_r * info->block_O_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*destination*/
                                    info->L1_O2, /*source*/
                                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                                    info->d * DATA_TYPE_BYTE, /*source stride*/
                                    info->Br_s /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                            }
                            flex_dma_async_1d(info->L1_O2,zomem(0),info->L1_O_size);
                            flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        }
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->HBM_CN + b * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }






                /********************************************************************
                 *  9. S * V Phase                                                  *
                 *     -- (if c == 0 && r !=0 ) store & (c==0)rest L1_O3            *
                 *     -- load      CN_2 --> L1_V1,                                 *
                 *     -- multicast CN_2                                            *
                 *     -- Compute   L1_S1, CN_1 --> L1_O2                           *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                }
                if (info->spatz_attached && c != 0)
                {
                    //Compute O = O*e
                    flat_attention_vector_M_mul_V(
                        info->d,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O3,
                        info->L1SP_e);
                }
                if (info->spatz_attached && c == (info->Tc - 1))
                {
                    //Compute O = O/l
                    flat_attention_vector_M_div_V(
                        info->d,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O1,
                        info->L1SP_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        if (c == 0)
                        {
                            if (store_enable)
                            {
                                //Reduction O
                                flex_dma_async_reduction(
                                    info->L1_O3/*dst_offset*/,
                                    info->L1_O3/*src_offset*/,
                                    info->L1_O_size/*transfer_size*/,
                                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                                    info->group.wakeup_row_mask/*row_mask*/,
                                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //Store O
                                flex_dma_async_2d(
                                    info->HBM_O + store_b * info->batch_O_size/*head offset*/ + store_r * info->block_O_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*destination*/
                                    info->L1_O3, /*source*/
                                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                                    info->d * DATA_TYPE_BYTE, /*source stride*/
                                    info->Br_s /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                            }
                            flex_dma_async_1d(info->L1_O3,zomem(0),info->L1_O_size);
                            flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        }
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->HBM_CN + b * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }






                /********************************************************************
                 *  10. S * V Phase                                                 *
                 *     -- (if c == 0 && r !=0 ) store & (c==0)rest L1_O4            *
                 *     -- load      CN_3 --> L1_V2,                                 *
                 *     -- multicast CN_3                                            *
                 *     -- Compute   L1_S1, CN_2 --> L1_O3                           *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V1, info->L1_O3, REDMULE_COMPUTE_TYPE);
                }
                if (info->spatz_attached && c != 0)
                {
                    //Compute O = O*e
                    flat_attention_vector_M_mul_V(
                        info->d,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O4,
                        info->L1SP_e);
                }
                if (info->spatz_attached && c == (info->Tc - 1))
                {
                    //Compute O = O/l
                    flat_attention_vector_M_div_V(
                        info->d,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O2,
                        info->L1SP_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        if (c == 0)
                        {
                            if (store_enable)
                            {
                                //Reduction O
                                flex_dma_async_reduction(
                                    info->L1_O4/*dst_offset*/,
                                    info->L1_O4/*src_offset*/,
                                    info->L1_O_size/*transfer_size*/,
                                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                                    info->group.wakeup_row_mask/*row_mask*/,
                                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //Store O
                                flex_dma_async_2d(
                                    info->HBM_O + store_b * info->batch_O_size/*head offset*/ + store_r * info->block_O_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*destination*/
                                    info->L1_O4, /*source*/
                                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                                    info->d * DATA_TYPE_BYTE, /*source stride*/
                                    info->Br_s /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                            }
                            flex_dma_async_1d(info->L1_O4,zomem(0),info->L1_O_size);
                            flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        }
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->HBM_CN + b * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }






                /********************************************************************
                 *  11. S * V Phase                                                 *
                 *     -- Compute   L1_S1, CN_3 --> L1_O4                           *
                 ********************************************************************/
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V2, info->L1_O4, REDMULE_COMPUTE_TYPE);
                }
                if (info->spatz_attached && c == (info->Tc - 1))
                {
                    //Compute O = O/l
                    flat_attention_vector_M_div_V(
                        info->d,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O3,
                        info->L1SP_l
                        );
                }
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (info->spatz_attached && c == (info->Tc - 1))
                {
                    //Compute O = O/l
                    flat_attention_vector_M_div_V(
                        info->d,
                        info->Br_s / ARCH_SPATZ_ATTACED_CORES,
                        info->L1SP_O4,
                        info->L1SP_l
                        );
                }
                grid_sync_group_barrier_xy(&(info->group));
            }
            store_enable = 1;
            store_b = b;
            store_r = r;
        }
    }

    /********************************************************************
     *  13. Last stores                                                 *
     *     -- store L1_O1, L1_O2, L1_O3, L1_O4                          *
     ********************************************************************/
    if (flex_is_dm_core())
    {
        if (info->cluster_for_rowwise == 1)
        {
            if (store_enable)
            {
                //Reduction O
                flex_dma_async_reduction(
                    info->L1_O1/*dst_offset*/,
                    info->L1_O1/*src_offset*/,
                    info->L1_O_size/*transfer_size*/,
                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                    info->group.wakeup_row_mask/*row_mask*/,
                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                //Store O
                flex_dma_async_2d(
                    info->HBM_O + store_b * info->batch_O_size/*head offset*/ + store_r * info->block_O_size/*i block offset*/ + 0 * info->d * DATA_TYPE_BYTE, /*destination*/
                    info->L1_O1, /*source*/
                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                    info->d * DATA_TYPE_BYTE, /*source stride*/
                    info->Br_s /*repeat*/); //Start 2D iDMA
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                //Reduction O
                flex_dma_async_reduction(
                    info->L1_O2/*dst_offset*/,
                    info->L1_O2/*src_offset*/,
                    info->L1_O_size/*transfer_size*/,
                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                    info->group.wakeup_row_mask/*row_mask*/,
                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                //Store O
                flex_dma_async_2d(
                    info->HBM_O + store_b * info->batch_O_size/*head offset*/ + store_r * info->block_O_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*destination*/
                    info->L1_O2, /*source*/
                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                    info->d * DATA_TYPE_BYTE, /*source stride*/
                    info->Br_s /*repeat*/); //Start 2D iDMA
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                //Reduction O
                flex_dma_async_reduction(
                    info->L1_O3/*dst_offset*/,
                    info->L1_O3/*src_offset*/,
                    info->L1_O_size/*transfer_size*/,
                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                    info->group.wakeup_row_mask/*row_mask*/,
                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                //Store O
                flex_dma_async_2d(
                    info->HBM_O + store_b * info->batch_O_size/*head offset*/ + store_r * info->block_O_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*destination*/
                    info->L1_O3, /*source*/
                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                    info->d * DATA_TYPE_BYTE, /*source stride*/
                    info->Br_s /*repeat*/); //Start 2D iDMA
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                //Reduction O
                flex_dma_async_reduction(
                    info->L1_O4/*dst_offset*/,
                    info->L1_O4/*src_offset*/,
                    info->L1_O_size/*transfer_size*/,
                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                    info->group.wakeup_row_mask/*row_mask*/,
                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                //Store O
                flex_dma_async_2d(
                    info->HBM_O + store_b * info->batch_O_size/*head offset*/ + store_r * info->block_O_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*destination*/
                    info->L1_O4, /*source*/
                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                    info->d * DATA_TYPE_BYTE, /*source stride*/
                    info->Br_s /*repeat*/); //Start 2D iDMA
                flex_dma_async_wait_all(); // Wait for iDMA Finishing
            }
        }
    }
    grid_sync_group_barrier_xy(&(info->group));
}


void FlatMLA_ayncrun(FlatMLAInfo * info)
{
    uint32_t recorded_store = 0;
    uint64_t recorded_store_addr;

    //Initialize with zeros
    if (flex_is_dm_core())
    {
        flex_dma_async_1d(info->L1_O1,zomem(0),info->L1_O_size);
        flex_dma_async_1d(info->L1_O2,zomem(0),info->L1_O_size);
        flex_dma_async_wait_all(); // Wait for iDMA Finishing
    }
    grid_sync_group_barrier_xy(&(info->group));

    //FlatMLA
    for (int b = 0; b < (info->work_group_batch / 2); ++b)
    {
        for (int r = 0; r < info->Tr; ++r)
        {
            for (int c = 0; c < info->Tc; ++c)
            {
                /************************/
                /*                      */
                /*      Phase 1-1       */
                /*      Preloading      */
                /*                      */
                /************************/

                //Preloading
                if (flex_is_dm_core())
                {
                    flex_dma_async_1d(info->L1_S1,zomem(0),info->L1_A_size);
                    flex_dma_async_1d(info->L1_S2,zomem(0),info->L1_A_size);
                    flex_dma_async_1d(info->L1_m,zomem(0),info->L1_m_size);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->HBM_QN + (2 * b) * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->HBM_CN + (2 * b) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/,/*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K1,info->L1_K1,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-1       */
                /*      B1_QK_1         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->HBM_QN + (2 * b) * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->HBM_CN + (2 * b) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K2,info->L1_K2,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }

                    if (recorded_store != 0)
                    {
                        if (flex_is_dm_core())
                        {
                            if (info->cluster_for_rowwise == 1)
                            {
                                //Reduction O
                                flex_dma_async_reduction(
                                    info->L1_O2/*dst_offset*/,
                                    info->L1_O2/*src_offset*/,
                                    info->L1_O_size/*transfer_size*/,
                                    COLLECTIVE_REDSUM_TYPE/*fmt*/,
                                    info->group.wakeup_row_mask/*row_mask*/,
                                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //Store O
                                flex_dma_async_2d(
                                    recorded_store_addr, /*destination*/
                                    info->L1_O2, /*source*/
                                    info->d * DATA_TYPE_BYTE, /*transfer size*/
                                    info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                                    info->d * DATA_TYPE_BYTE, /*source stride*/
                                    info->Br_s /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                flex_dma_async_1d(info->L1_O2,zomem(0),info->L1_O_size);
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                            }
                        }
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-2       */
                /*      B1_QK_2         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->HBM_QN + (2 * b) * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->HBM_CN + (2 * b) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K1,info->L1_K1,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-3       */
                /*      B1_QK_3         */
                /*                      */
                /************************/


                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->HBM_QN + (2 * b) * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->HBM_CN + (2 * b) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K2,info->L1_K2,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-4       */
                /*      B1_QK_4         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->HBM_QR + (2 * b) * info->batch_QR_size/*head offset*/ + r * info->block_QR_size/*i block offset*/, /*source*/
                            info->d_rope * DATA_TYPE_BYTE, /*transfer size*/
                            info->d_rope * DATA_TYPE_BYTE, /*destination stride*/
                            info->d_rope * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->Br_s * info->d_rope * DATA_TYPE_BYTE/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->HBM_CR + (2 * b) * info->batch_CR_size/*head offset*/ + c * info->block_CR_size/*i block offset*/, /*source*/
                            info->d_rope * DATA_TYPE_BYTE, /*transfer size*/
                            info->d_rope * DATA_TYPE_BYTE, /*destination stride*/
                            info->d_rope * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d_rope,info->L1_K1,info->L1_K1,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->Bc_s * info->d_rope * DATA_TYPE_BYTE/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-5       */
                /*      B1_QK_5         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d_rope, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->HBM_QN + (2 * b + 1) * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 0 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->HBM_CN + (2 * b + 1) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 0 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K2,info->L1_K2,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 3-1       */
                /*      B2_QK_1         */
                /*                      */
                /************************/


                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S2, REDMULE_COMPUTE_TYPE);
                    //Local Maxima
                    flat_attention_vector_rowmax_MV_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S1,
                        info->L1_m,
                        info->L1_m,
                        info->sqrt_dim
                    );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->HBM_QN + (2 * b + 1) * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->HBM_CN + (2 * b + 1) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K1,info->L1_K1,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_dm_core())
                {
                    //Global maxima
                    flex_dma_async_reduction(
                        info->L1_m/*dst_offset*/,
                        info->L1_m/*src_offset*/,
                        info->L1_m_size/*transfer_size*/,
                        COLLECTIVE_REDMAX_TYPE/*fmt*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    flex_dma_async_broadcast(
                        info->L1_m/*dst_offset*/,
                        info->L1_m/*src_offset*/,
                        info->L1_m_size/*transfer_size*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }


                /************************/
                /*                      */
                /*      Phase 3-2       */
                /*      B2_QK_2         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S2, REDMULE_COMPUTE_TYPE);
                    //p = exp(att - m)
                    flat_attention_vector_EXP_MV(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S1,
                        info->L1_m
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->HBM_QN + (2 * b + 1) * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->HBM_CN + (2 * b + 1) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K2,info->L1_K2,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 3-3       */
                /*      B2_QK_3         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S2, REDMULE_COMPUTE_TYPE);
                    //l = rowsum(p)
                    flat_attention_vector_rowsum_M_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S1,
                        info->L1_l
                        );
                }
                if (flex_is_dm_core())
                {
                    flex_dma_async_1d(info->L1_m,zomem(0),info->L1_m_size);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->HBM_QN + (2 * b + 1) * info->batch_QN_size/*head offset*/ + r * info->block_QN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->HBM_CN + (2 * b + 1) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d,info->L1_K1,info->L1_K1,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_dm_core())
                {
                    //Global Denominator
                    flex_dma_async_reduction(
                        info->L1_l/*dst_offset*/,
                        info->L1_l/*src_offset*/,
                        info->L1_l_size/*transfer_size*/,
                        COLLECTIVE_REDSUM_TYPE/*fmt*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    flex_dma_async_broadcast(
                        info->L1_l/*dst_offset*/,
                        info->L1_l/*src_offset*/,
                        info->L1_l_size/*transfer_size*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }


                /************************/
                /*                      */
                /*      Phase 3-4       */
                /*      B2_QK_4         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S2, REDMULE_COMPUTE_TYPE);
                    //S = S/l
                    flat_attention_vector_M_div_V(
                        info->Br_s,
                        info->Bc_s,
                        info->L1_S1,
                        info->L1_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->HBM_QR + (2 * b + 1) * info->batch_QR_size/*head offset*/ + r * info->block_QR_size/*i block offset*/, /*source*/
                            info->d_rope * DATA_TYPE_BYTE, /*transfer size*/
                            info->d_rope * DATA_TYPE_BYTE, /*destination stride*/
                            info->d_rope * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->Br_s * info->d_rope * DATA_TYPE_BYTE/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->HBM_CR + (2 * b + 1) * info->batch_CR_size/*head offset*/ + c * info->block_CR_size/*i block offset*/, /*source*/
                            info->d_rope * DATA_TYPE_BYTE, /*transfer size*/
                            info->d_rope * DATA_TYPE_BYTE, /*destination stride*/
                            info->d_rope * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //transpose K
                        flex_transpose_engine_config(info->Bc_s,info->d_rope,info->L1_K2,info->L1_K2,DATA_TYPE_BYTE);
                        flex_transpose_engine_trigger();
                        flex_transpose_engine_wait();
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->Bc_s * info->d_rope * DATA_TYPE_BYTE/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 3-5       */
                /*      B2_QK_5         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d_rope, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S2, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->HBM_CN + (2 * b) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 0 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 4-1       */
                /*      B1_SV_1         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->Bc_s, info->d);
                    flex_redmule_trigger(info->L1_S1, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                    //Local Maxima
                    flat_attention_vector_rowmax_MV_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S2,
                        info->L1_m,
                        info->L1_m,
                        info->sqrt_dim
                    );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->HBM_CN + (2 * b) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_dm_core())
                {
                    //Global maxima
                    flex_dma_async_reduction(
                        info->L1_m/*dst_offset*/,
                        info->L1_m/*src_offset*/,
                        info->L1_m_size/*transfer_size*/,
                        COLLECTIVE_REDMAX_TYPE/*fmt*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    flex_dma_async_broadcast(
                        info->L1_m/*dst_offset*/,
                        info->L1_m/*src_offset*/,
                        info->L1_m_size/*transfer_size*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }


                /************************/
                /*                      */
                /*      Phase 4-2       */
                /*      B1_SV_2         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                    //p = exp(att - m)
                    flat_attention_vector_EXP_MV(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S2,
                        info->L1_m
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->HBM_CN + (2 * b) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_rowwise == 1)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O1/*dst_offset*/,
                            info->L1_O1/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDSUM_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->HBM_O + (2 * b) * info->batch_O_size/*head offset*/ + r * info->block_O_size/*i block offset*/ + 0 * info->d * DATA_TYPE_BYTE, /*destination*/
                            info->L1_O1, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                            info->d * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_1d(info->L1_O1,zomem(0),info->L1_O_size);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 4-3       */
                /*      B1_SV_3         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                    //l = rowsum(p)
                    flat_attention_vector_rowsum_M_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S2,
                        info->L1_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->HBM_CN + (2 * b) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_rowwise == 1)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O2/*dst_offset*/,
                            info->L1_O2/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDSUM_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->HBM_O + (2 * b) * info->batch_O_size/*head offset*/ + r * info->block_O_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*destination*/
                            info->L1_O2, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                            info->d * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_1d(info->L1_O2,zomem(0),info->L1_O_size);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_dm_core())
                {
                    //Global Denominator
                    flex_dma_async_reduction(
                        info->L1_l/*dst_offset*/,
                        info->L1_l/*src_offset*/,
                        info->L1_l_size/*transfer_size*/,
                        COLLECTIVE_REDSUM_TYPE/*fmt*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    flex_dma_async_broadcast(
                        info->L1_l/*dst_offset*/,
                        info->L1_l/*src_offset*/,
                        info->L1_l_size/*transfer_size*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }


                /************************/
                /*                      */
                /*      Phase 4-4       */
                /*      B1_SV_4         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                    //S = S/l
                    flat_attention_vector_M_div_V(
                        info->Br_s,
                        info->Bc_s,
                        info->L1_S2,
                        info->L1_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->HBM_CN + (2 * b + 1) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 0 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_rowwise == 1)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O1/*dst_offset*/,
                            info->L1_O1/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDSUM_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->HBM_O + (2 * b) * info->batch_O_size/*head offset*/ + r * info->block_O_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*destination*/
                            info->L1_O1, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                            info->d * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_1d(info->L1_O1,zomem(0),info->L1_O_size);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 5-1       */
                /*      B2_SV_1         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S2, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->HBM_CN + (2 * b + 1) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_rowwise == 1)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O2/*dst_offset*/,
                            info->L1_O2/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDSUM_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->HBM_O + (2 * b) * info->batch_O_size/*head offset*/ + r * info->block_O_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*destination*/
                            info->L1_O2, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                            info->d * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_1d(info->L1_O2,zomem(0),info->L1_O_size);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 5-2       */
                /*      B2_SV_2         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S2, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->HBM_CN + (2 * b + 1) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_rowwise == 1)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O1/*dst_offset*/,
                            info->L1_O1/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDSUM_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->HBM_O + (2 * b + 1) * info->batch_O_size/*head offset*/ + r * info->block_O_size/*i block offset*/ + 0 * info->d * DATA_TYPE_BYTE, /*destination*/
                            info->L1_O1, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                            info->d * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_1d(info->L1_O1,zomem(0),info->L1_O_size);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 5-3       */
                /*      B2_SV_3         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S2, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_colwise == 1)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->HBM_CN + (2 * b + 1) * info->batch_CN_size/*head offset*/ + c * info->block_CN_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->d * DATA_TYPE_BYTE, /*destination stride*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_for_rowwise == 1)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O2/*dst_offset*/,
                            info->L1_O2/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDSUM_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->HBM_O + (2 * b + 1) * info->batch_O_size/*head offset*/ + r * info->block_O_size/*i block offset*/ + 1 * info->d * DATA_TYPE_BYTE, /*destination*/
                            info->L1_O2, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                            info->d * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_1d(info->L1_O2,zomem(0),info->L1_O_size);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 5-4       */
                /*      B2_SV_4         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S2, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_for_rowwise == 1)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O1/*dst_offset*/,
                            info->L1_O1/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDSUM_TYPE/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->HBM_O + (2 * b + 1) * info->batch_O_size/*head offset*/ + r * info->block_O_size/*i block offset*/ + 2 * info->d * DATA_TYPE_BYTE, /*destination*/
                            info->L1_O1, /*source*/
                            info->d * DATA_TYPE_BYTE, /*transfer size*/
                            info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                            info->d * DATA_TYPE_BYTE, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        flex_dma_async_1d(info->L1_O1,zomem(0),info->L1_O_size);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }

                //Record Store Address
                recorded_store = 1;
                recorded_store_addr = info->HBM_O + (2 * b + 1) * info->batch_O_size/*head offset*/ + r * info->block_O_size/*i block offset*/ + 3 * info->d * DATA_TYPE_BYTE;
            }
        }
    }

    /************************/
    /*                      */
    /*      Phase 6-1       */
    /*      Last Store      */
    /*                      */
    /************************/
    grid_sync_group_barrier_xy(&(info->group));
    if (flex_is_first_core())
    {
        flex_redmule_wait();
    }
    flex_intra_cluster_sync();
    if (flex_is_dm_core())
    {
        if (info->cluster_for_rowwise == 1)
        {
            //Reduction O
            flex_dma_async_reduction(
                info->L1_O2/*dst_offset*/,
                info->L1_O2/*src_offset*/,
                info->L1_O_size/*transfer_size*/,
                COLLECTIVE_REDSUM_TYPE/*fmt*/,
                info->group.wakeup_row_mask/*row_mask*/,
                (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
            //Store O
            flex_dma_async_2d(
                recorded_store_addr, /*destination*/
                info->L1_O2, /*source*/
                info->d * DATA_TYPE_BYTE, /*transfer size*/
                info->nope_head_dim * DATA_TYPE_BYTE, /*destination stride*/
                info->d * DATA_TYPE_BYTE, /*source stride*/
                info->Br_s /*repeat*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
    }
    grid_sync_group_barrier_xy(&(info->group));

}


#define REPEAT_1(x) x
#define REPEAT_2(x) REPEAT_1(x) x
#define REPEAT_4(x) REPEAT_2(x) REPEAT_2(x)
#define REPEAT_8(x) REPEAT_4(x) REPEAT_4(x)
#define REPEAT_16(x) REPEAT_8(x) REPEAT_8(x)
#define REPEAT_32(x) REPEAT_16(x) REPEAT_16(x)
#define REPEAT_64(x) REPEAT_32(x) REPEAT_32(x)
#define REPEAT_128(x) REPEAT_64(x) REPEAT_64(x)


inline void flat_attention_vector_rowmax_MV_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src, uint32_t V_dst, tmla_data_t sqrt_dim){
    uint32_t vl;
    asm volatile("fld fa5, (%0)" ::"r"(&sqrt_dim));
    asm volatile("li a7, 0":::"a7");
    //load V_src
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(V_src));
    //Scale and Redmax for each row of M_src
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));\
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfmul.vf v8, v8, fa5");\
            asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile(".word %0\n"::"i"(0x1e88b057):"a7") /*vfredmax.vx v0, a7, v8 --> Customized Instruction: v0[a7] = v0[a7] + RedMax(v8)*/; \
            asm volatile("addi a7, a7, 1":::"a7"); \
            M_src += num_cols * 2;\
        )
    }
    //Store back max value
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(V_dst));
}


inline void flat_attention_vector_EXP_MV(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("fld fa5, (%0)" ::"r"(V_src));\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfsub.vf v8, v8, fa5");\
            asm volatile(".word %0\n"::"i"(0x32041857)) /*vfexp.vv v16, v8*/; \
            asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(M_src));\
            M_src += num_cols * 2;\
            V_src += 2;\
        )
    }
}

inline void flat_attention_vector_EXP_VV_V(uint32_t vector_length, uint32_t Vr_src, uint32_t V_src, uint32_t V_dst){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(vector_length));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(Vr_src));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(V_src));
    asm volatile("vfsub.vv v8, v0, v8");
    asm volatile(".word %0\n"::"i"(0x32041857)) /*vfexp.vv v16, v8*/; \
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(V_dst));
}


inline void flat_attention_vector_rowsum_M_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_dst){
    uint32_t vl;
    asm volatile("li a7, 0":::"a7");
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vmv.v.x v0, %0" ::"r"(0));
    //Redsum for each row of M_src
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));\
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile(".word %0\n"::"i"(0x0688b057):"a7") /*vfredsum.vx v0, a7, v8 --> Customized Instruction: v0[a7] = v0[a7] + RedSum(v8)*/; \
            asm volatile("addi a7, a7, 1":::"a7"); \
            M_src += num_cols * 2;\
        )
    }
    //Store back max value
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_rows));
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(V_dst));
}

//l = e-*-lr + l
inline void flat_attention_vector_update_l(uint32_t vector_length, uint32_t lr, uint32_t e, uint32_t l){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(vector_length));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(lr));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(e));
    asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(l));
    asm volatile("vfmul.vv v8, v8, v0");
    asm volatile("vfadd.vv v16, v16, v8");
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(l));
}

void flat_attention_vector_M_div_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("fld fa5, (%0)" ::"r"(V_src));\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfdiv.vf v8, v8, fa5");\
            asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            M_src += num_cols * 2;\
            V_src += 2;\
        )
    }
}

void flat_attention_vector_M_mul_V(uint32_t num_cols, uint32_t num_rows, uint32_t M_src, uint32_t V_src){
    uint32_t vl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(vl) : "r"(num_cols));
    for (int i = 0; i < (num_rows/16); ++i)
    {
        REPEAT_16(\
            asm volatile("fld fa5, (%0)" ::"r"(V_src));\
            asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            asm volatile("vfmul.vf v8, v8, fa5");\
            asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(M_src));\
            M_src += num_cols * 2;\
            V_src += 2;\
        )
    }
}


#endif  