// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 6.Jan.2025

#ifndef _FLAT_ATTENTION_H_
#define _FLAT_ATTENTION_H_

#include "FlatAttentionUtil.h"

void flatcoll_run(FlatAttentionInfo* info);
void flatasync_run(FlatAttentionInfo* info);

int flat_attention(
    uint32_t                sequence_length,
    uint32_t                head_dimemsion,
    uint32_t                head_num,
    uint32_t                batch_size,
    uint32_t                elem_size,
    uint32_t                flatten_scale,
    uint32_t                flatten_shape,
    uint32_t                async_enable,
    uint32_t                dump_enable){

    //analyze flatten attention settings
    flex_global_barrier_xy();
    FlatAttentionInfo info = flat_attention_analyze(
                                    sequence_length,
                                    head_dimemsion,
                                    head_num,
                                    batch_size,
                                    elem_size,
                                    flatten_scale,
                                    flatten_shape);
    if (info.flat_attention_valid == 0)
    {
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
        {
            printf("\033[1;31m[Configuration Error] please double check your arch and attn configuration\033[0m\n");
        }
        return 1;
    }
    flex_global_barrier_xy();

    //execute flatten attention
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    if (async_enable)
    {
        flatasync_run(&info);
    } else {
        flatcoll_run(&info);
    }
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

    //dump results
    if (dump_enable)
    {
        //postload part of O
        if (flex_get_cluster_id() == 0 && flex_is_dm_core())
        {
            flex_dump_open();
            flex_dump_hbm(info.HBM_O - ARCH_HBM_START_BASE, 1024);
            flex_dump_close();
        }
    }
    flex_global_barrier_xy();

    return 0;
}

void flatcoll_run(FlatAttentionInfo* info){
    for (int heads = 0; heads < info->work_group_head_num; ++heads)
    {
        for (int i = 0; i < info->Tr; ++i)
        {
            for (int j = 0; j < info->Tc; ++j)
            {
                /********************************************************************
                 *  1. initialization DMA Actions                                   *
                 *     -- initialize L1_A, L1_O, L1_mr, L1_lr                       *
                 *     -- load Qi_sy, KTj_sx, Vj_sx                                 *
                 ********************************************************************/
                if (flex_is_dm_core()){
                    flex_dma_async_1d(info->L1_A,zomem(0),info->L1_A_size);
                    flex_dma_async_wait_all();
                    if (j == 0) {
                        flex_dma_async_1d(info->L1_mr,zomem(0),info->L1_m_size+info->L1_l_size);
                        flex_dma_async_wait_all();
                        flex_dma_async_1d(
                            info->L1_O,
                            zomem(0),
                            info->L1_O_size);
                        flex_dma_async_wait_all();
                    } else {
                        //register m => mr, register l => lr
                        flex_dma_async_1d(info->L1_mr,info->L1_m,info->L1_m_size+info->L1_l_size);
                        flex_dma_async_wait_all();
                    }

                    if (info->slice_is_x_edge && j == 0) {
                        //load Qi_sy
                        flex_dma_async_1d(
                            info->L1_Q,
                            info->HBM_Q + (i + heads*info->Tr) * info->slice_QO_size,
                            info->L1_Q_size);
                        flex_dma_async_wait_all();
                    }


                    if (info->slice_is_y_edge)
                    {
                        //load Vj_sx
                        flex_dma_async_1d(
                            info->L1_V,
                            info->HBM_V + (j + heads*info->Tc) * info->slice_KTV_size,
                            info->L1_V_size);
                        flex_dma_async_wait_all();

                        //load KTj_sx
                        flex_dma_async_1d(
                            info->L1_KT,
                            info->HBM_KT + (j + heads*info->Tc) * info->slice_KTV_size,
                            info->L1_KT_size);
                        flex_dma_async_wait_all();
                    }
                }
                flex_intra_cluster_sync();



                /********************************************************************
                 *  2.Broadcast Qi_sy, KTj_sx, Vj_sx                                *
                 ********************************************************************/
                if (j == 0 && info->flatten_slice_x > 1) {
                    grid_sync_group_barrier_xy(&(info->group));
                    flat_attention_broadcast_rowwise(
                        info,
                        info->L1_Q,
                        info->L1_Q_size);
                    grid_sync_group_barrier_xy(&(info->group));
                }
                if (info->flatten_slice_y > 1) {
                    grid_sync_group_barrier_xy(&(info->group));
                    flat_attention_broadcast_colwise(
                        info,
                        info->L1_KT,
                        info->L1_KT_size + info->L1_Q_size);
                    grid_sync_group_barrier_xy(&(info->group));
                }



                /********************************************************************
                 *  3.Calculate attention matrix and rowmax                         *
                 *    -- Att = (Qi_sy * KTj_sx) / scale_factor                      *
                 *       -- (omit scaling here)                                     *
                 *    -- m = max(mr, rowmax(Att))                                   *
                 ********************************************************************/
                if (flex_is_first_core())
                {
                    //Att = (Qi_sy * KTj_sx)
                    flex_redmule_config(info->Br_s, info->d, info->Bc_s);
                    flex_redmule_trigger(
                        info->L1_Q,
                        info->L1_KT,
                        info->L1_A,
                        REDMULE_FP_16);
                    flex_redmule_wait();
                    // m = max(mr, rowmax(Att))
                    flat_attention_vector_rowmax_MV_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_A,
                        info->L1_mr,
                        info->L1_m
                        );
                }
                flex_intra_cluster_sync();



                /********************************************************************
                 *  4. Rowmax reducetion between clusters and broadcast back        *
                 ********************************************************************/
                if (info->flatten_slice_x > 1) {
                    grid_sync_group_barrier_xy(&(info->group));
                    flat_attention_redmax_rowwise(
                        info,
                        info->L1_m,
                        info->L1_m_size);
                    grid_sync_group_barrier_xy(&(info->group));
                    flat_attention_broadcast_rowwise(
                        info,
                        info->L1_m,
                        info->L1_m_size);
                    grid_sync_group_barrier_xy(&(info->group));
                }



                /********************************************************************
                 *  5. Vector operations for softmax after obtaining global rowmax  *
                 *     -- p = exp(att - m)                                          *
                 *     -- if(j!=0) e = exp(mr - m)                                  *
                 *     -- l = rowsum(p)                                             *
                 ********************************************************************/
                if (flex_is_first_core())
                {
                    //p = exp(att - m)
                    flat_attention_vector_EXP_MV(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_A,
                        info->L1_m
                        );

                    //l = rowsum(p)
                    flat_attention_vector_rowsum_M_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_A,
                        info->L1_l
                        );
                }
                flex_intra_cluster_sync();



                /********************************************************************
                 *  6. Rowsum reducetion between clusters and broadcast back        *
                 ********************************************************************/
                if (info->flatten_slice_x > 1) {
                    grid_sync_group_barrier_xy(&(info->group));
                    flat_attention_redsum_rowwise(
                        info,
                        info->L1_l,
                        info->L1_l_size);
                    grid_sync_group_barrier_xy(&(info->group));
                    flat_attention_broadcast_rowwise(
                        info,
                        info->L1_l,
                        info->L1_l_size);
                    grid_sync_group_barrier_xy(&(info->group));
                }


                /********************************************************************
                 *  7.Calculate O = O + p*Vj                                        *
                 *    -- if(j != 0) e = exp(mr - m); l = e-*-lr + l; O = O/e        *
                 *    -- O = O + p*Vj                                               *
                 ********************************************************************/
                if (flex_is_first_core())
                {
                    if (j != 0)
                    {
                        //Compute e = exp(mr - m)
                        flat_attention_vector_EXP_VV_V(
                            info->Br_s,
                            info->L1_mr,
                            info->L1_m,
                            info->L1_e);

                        //Compute l = e-*-lr + l
                        flat_attention_vector_update_l(
                            info->Br_s,
                            info->L1_lr,
                            info->L1_e,
                            info->L1_l);

                        //Compute O = O/e
                        flat_attention_vector_M_div_V(
                            info->d,
                            info->Br_s,
                            info->L1_O,
                            info->L1_e);
                    }
                    //O = O + p*Vj
                    flex_redmule_config(info->Br_s, info->Bc_s, info->d);
                    flex_redmule_trigger(
                        info->L1_A,
                        info->L1_V,
                        info->L1_O,
                        REDMULE_FP_16);
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
            }

            /********************************************************************
             *  9. O = O/l                                                      *
             ********************************************************************/
            if (flex_is_first_core()){
                //O = O/l
                flat_attention_vector_M_div_V(
                    info->d,
                    info->Br_s,
                    info->L1_O,
                    info->L1_l
                    );
            }
            flex_intra_cluster_sync();


            /********************************************************************
             *  10. Output reducetion between clusters                           *
             ********************************************************************/
            if (info->flatten_slice_x > 1) {
                grid_sync_group_barrier_xy(&(info->group));
                flat_attention_redsum_rowwise(
                    info,
                    info->L1_O,
                    info->L1_O_size);
                grid_sync_group_barrier_xy(&(info->group));
            }

            if (info->slice_is_x_edge)
            {
                //11. Store O
                if(flex_is_dm_core()){
                    flex_dma_async_1d(
                        info->HBM_O + (heads*info->Tr + i) * info->slice_QO_size,
                        info->L1_O,
                        info->L1_O_size);
                    flex_dma_async_wait_all();
                }
            }
            flex_intra_cluster_sync();
        }
    }
}



typedef struct AttetionIteration
{
    int h; int i; int j; int hn; int tr; int tc;
}AttetionIteration;

AttetionIteration ai_get_previous(AttetionIteration* attn_iter){
    AttetionIteration res;
    res.h  = attn_iter->h;
    res.i  = attn_iter->i;
    res.j  = attn_iter->j;
    res.hn = attn_iter->hn;
    res.tr = attn_iter->tr;
    res.tc = attn_iter->tc;
    res.j  = res.j - 1;
    if (res.j < 0)
    {
        res.j = res.tc - 1;
        res.i = res.i - 1;
        if (res.i < 0)
        {
            res.i = res.tr - 1;
            res.h = res.h - 1;
        }
    }
    return res;
}

AttetionIteration ai_get_next(AttetionIteration* attn_iter){
    AttetionIteration res;
    res.h  = attn_iter->h;
    res.i  = attn_iter->i;
    res.j  = attn_iter->j;
    res.hn = attn_iter->hn;
    res.tr = attn_iter->tr;
    res.tc = attn_iter->tc;
    res.j  = res.j + 1;
    if (res.j >= res.tc)
    {
        res.j = 0;
        res.i = res.i + 1;
        if (res.i >= res.tr)
        {
            res.i = 0;
            res.h = res.h + 1;
        }
    }
    return res;
}

#define ai_is_valid(attn_iter) (attn_iter.h >= 0 && attn_iter.i >= 0 && attn_iter.j >= 0 && attn_iter.h < attn_iter.hn && attn_iter.i < attn_iter.tr && attn_iter.j < attn_iter.tc)

void flatasync_run(FlatAttentionInfo* info){
    AttetionIteration attn_iter;    //< n >
    AttetionIteration _attn_iter;   //<n-1>
    AttetionIteration __attn_iter;  //<n-2>
    AttetionIteration attn_iter_;   //<n+1>
    /*
    * Initialization
    */
    attn_iter.h  = 0;
    attn_iter.i  = 0;
    attn_iter.j  = 0;
    attn_iter.hn = info->work_group_head_num/2;
    attn_iter.tr = info->Tr;
    attn_iter.tc = info->Tc;
    _attn_iter = ai_get_previous(&attn_iter);
    __attn_iter = ai_get_previous(&_attn_iter);
    attn_iter_ = ai_get_next(&attn_iter);

    uint32_t valid = ai_is_valid(attn_iter);
    uint32_t _valid = ai_is_valid(_attn_iter);
    uint32_t __valid = ai_is_valid(__attn_iter);
    uint32_t valid_ = ai_is_valid(attn_iter_);

    int iter_j = attn_iter.j;
    int _iter_j = _attn_iter.j;
    int __iter_j = __attn_iter.j;
    int iter_j_ = attn_iter_.j;

    int iter_i = attn_iter.i;
    int _iter_i = _attn_iter.i;
    int __iter_i = __attn_iter.i;
    int iter_i_ = attn_iter_.i;

    int iter_h = attn_iter.h;
    int _iter_h = _attn_iter.h;
    int __iter_h = __attn_iter.h;
    int iter_h_ = attn_iter_.h;

    /********************************************************************************
     * PHASE 0: | RedMule               |   iDMA                | Spatz              |
     * ------------------------------------------------------------------------------|
     *          | Set B                 |   Set A                                    |
     *          |--------------------------------------------------------------------|
     *          |                       | < 0 > load Q_sy       |                    |
     *          |                       | < 0 > Broadcast Q_sy  |                    |
     *          |                       | < 0 > load K_sx       |                    |
     *          |                       | < 0 > Broadcast K_sx  |                    |
     ********************************************************************************/
    if (flex_is_dm_core())
    {
        flex_dma_async_1d(
            info->L1_O,
            zomem(0),
            info->L1_O_size);
        flex_dma_async_wait_all();

        flex_dma_async_1d(
            info->DB_L1_O,
            zomem(0),
            info->L1_O_size);
        flex_dma_async_wait_all();

        flex_dma_async_1d(info->L1_mr,zomem(0),info->L1_m_size+info->L1_l_size);
        flex_dma_async_wait_all();

        flex_dma_async_1d(info->DB_L1_mr,zomem(0),info->L1_m_size+info->L1_l_size);
        flex_dma_async_wait_all();

        if (info->slice_is_x_edge){
            flex_dma_async_1d(
                info->L1_Q,
                info->HBM_Q,
                info->L1_Q_size);
            flex_dma_async_wait_all();

            if (info->flatten_slice_x > 1)
            {
                flex_dma_async_1d_broadcast(
                    remote_xy(
                        info->group.this_grid_right_most,
                        info->cluster_pos.y,
                        info->L1_Q),
                    local(info->L1_Q),
                    info->L1_Q_size);
                flex_dma_async_wait_all();
            }
        }

        if (info->slice_is_y_edge)
        {
            flex_dma_async_1d(
                info->L1_KT,
                info->HBM_KT,
                info->L1_KT_size);
            flex_dma_async_wait_all();

            if (info->flatten_slice_y > 1)
            {
                flex_dma_async_1d_broadcast(
                    remote_xy(
                        info->cluster_pos.x,
                        info->group.this_grid_top_most,
                        info->L1_KT),
                    local(info->L1_KT),
                    info->L1_KT_size);
                flex_dma_async_wait_all();
            }
        }
    }
    grid_sync_group_barrier_xy(&(info->group));

    /*
    * Routine Iterations
    */
    for (int iter = 0; iter < ((info->work_group_head_num/2) * info->Tr * info->Tc + 2); ++iter)
    {
        /********************************************************************************
         * PHASE 1: | RedMule               |   iDMA                | Spatz              |
         * ------------------------------------------------------------------------------|
         *          | Set A                 |   Set B                                    |
         *          |--------------------------------------------------------------------|
         *          |  <n-1> O = O + P*V    | <n-2> RedSum O        |                    |
         *          |                       |--------------------------------------------|
         *          |                       | < n > load Q_sy       | <n-2> O=O/lr       |
         *          |                       | < n > Broadcast Q_sy  |                    |
         *          |                       |--------------------------------------------|
         *          |                       | <n-2> store O         | <n-1> m=rowmax(Att)|
         *          |                       |--------------------------------------------|
         *          |                       | <n-1> Redmax+Bcast m  |                    |
         *          |                       |--------------------------------------------|
         *          |                       |                       | <n-1> e=exp(mr-m)  |
         *          |                       |                       | <n-1> m => mr      |
         ********************************************************************************/

        //Set A: <n-1> O = O + P*V --- Start
        if (flex_is_first_core() && _valid){
            flex_redmule_config(info->Br_s, info->Bc_s, info->d);
            flex_redmule_trigger(
                info->L1_A,
                info->L1_V,
                info->L1_O,
                REDMULE_FP_16);
        }

        //Set B: <n-2> RedSum O
        if (flex_is_dm_core() && __valid && __iter_j == (info->Tc - 1))
        {
            if(info->cluster_pos.x == info->group.this_grid_right_most && info->flatten_slice_x > 1){
                flex_dma_async_1d_reduction(
                    remote_xy(
                        info->group.this_grid_left_most,
                        info->cluster_pos.y,
                        info->DB_L1_O),
                    local(info->DB_L1_O),
                    info->L1_O_size,
                    COLLECTIVE_REDADD_FP_16);
                flex_dma_async_wait_all();
            }
        }
        grid_sync_group_barrier_xy(&(info->group));

        //Set B: <n-2> O=O/lr
        if (flex_is_first_core() && __valid && __iter_j == (info->Tc - 1))
        {
            if (info->slice_is_x_edge){
                flat_attention_vector_M_div_V(
                    info->d,
                    info->Br_s,
                    info->DB_L1_O,
                    info->DB_L1_l);
            }
        }

        //Set B: < n > load Q_sy + < n > Broadcast Q_sy
        if (flex_is_dm_core() && valid && iter_j == 0)
        {
            if (info->slice_is_x_edge){
                flex_dma_async_1d(
                    info->DB_L1_Q,
                    info->HBM_Q + (iter_i + (2*iter_h + 1)*info->Tr) * info->slice_QO_size,
                    info->L1_Q_size);
                flex_dma_async_wait_all();

                if (info->flatten_slice_x > 1)
                {
                    flex_dma_async_1d_broadcast(
                        remote_xy(
                            info->group.this_grid_right_most,
                            info->cluster_pos.y,
                            info->DB_L1_Q),
                        local(info->DB_L1_Q),
                        info->L1_Q_size);
                    flex_dma_async_wait_all();
                }
            }
        }
        flex_intra_cluster_sync();

        //Set B: <n-1> m=rowmax(Att)
        if (flex_is_first_core() && _valid)
        {
            flat_attention_vector_rowmax_MV_V(
                info->Bc_s,
                info->Br_s,
                info->DB_L1_A,
                info->DB_L1_mr,
                info->DB_L1_m);
        }

        //Set B: <n-2> store O
        if (flex_is_dm_core() && __valid && __iter_j == (info->Tc - 1))
        {
            if (info->slice_is_x_edge){
                flex_dma_async_1d(
                    info->HBM_O + (__iter_i + (2*__iter_h + 1)*info->Tr) * info->slice_QO_size,
                    info->DB_L1_O,
                    info->L1_O_size);
                flex_dma_async_wait_all();
            }

            flex_dma_async_1d(info->DB_L1_mr,zomem(0),info->L1_m_size+info->L1_l_size);
            flex_dma_async_wait_all();

            flex_dma_async_1d(
                info->DB_L1_O,
                zomem(0),
                info->L1_O_size);
            flex_dma_async_wait_all();
        }

        //Set B: <n-1> Redmax+Bcast m
        if (_valid && info->flatten_slice_x > 1)
        {
            grid_sync_group_barrier_xy(&(info->group));
            flat_attention_redmax_rowwise(
                info,
                info->DB_L1_m,
                info->L1_m_size);
            grid_sync_group_barrier_xy(&(info->group));
            flat_attention_broadcast_rowwise(
                info,
                info->DB_L1_m,
                info->L1_m_size);
            grid_sync_group_barrier_xy(&(info->group));
        }

        //Set B: <n-1> e=exp(mr-m)
        if (_valid && _iter_j != 0)
        {
            if (flex_is_first_core())
            {
                flat_attention_vector_EXP_VV_V(
                    info->Br_s,
                    info->DB_L1_mr,
                    info->DB_L1_m,
                    info->DB_L1_e);
            }
            flex_intra_cluster_sync();
        }

        //Set B: <n-1> m => mr
        if (flex_is_dm_core() && _valid)
        {
            flex_dma_async_1d(info->DB_L1_mr,info->DB_L1_m,info->L1_m_size);
            flex_dma_async_wait_all();
        }

        //Set A: <n-1> O = O + P*V --- End
        if (flex_is_first_core() && _valid){
            flex_redmule_wait();
        }
        flex_intra_cluster_sync();

        /*
        * Clear Set A Att Matrix
        */
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(info->L1_A,zomem(0),info->L1_A_size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        /********************************************************************************
         * PHASE 2: | RedMule               |   iDMA                | Spatz              |
         * ------------------------------------------------------------------------------|
         *          | Set A                 |   Set B                                    |
         *          |--------------------------------------------------------------------|
         *          |  < n > Att = Q*KT     | < n > load K_sx       | <n-1> p=exp(Att-m) |
         *          |                       | <n-1> load V_sx       | <n-1> l=rowsum(p)  |
         *          |                       | < n > Broadcast K_sx  |                    |
         *          |                       | <n-1> Broadcast V_sx  |                    |
         *          |                       |--------------------------------------------|
         *          |                       | <n-1> Redsum+Bcast l  |                    |
         *          |                       |--------------------------------------------|
         *          |                       |                       | <n-1> l=e*lr+l     |
         *          |                       |                       | <n-1> O = O/e      |
         *          |                       |                       | <n-1> l => lr      |
         ********************************************************************************/

        //Set A: < n > Att = Q*KT --- Start
        if (flex_is_first_core() && valid){
            flex_redmule_config(info->Br_s, info->d, info->Bc_s);
            flex_redmule_trigger(
                info->L1_Q,
                info->L1_KT,
                info->L1_A,
                REDMULE_FP_16);
        }

        //Set B: <n-1> p=exp(Att-m) and <n-1> l=rowsum(p)
        if (flex_is_first_core() && _valid){
            flat_attention_vector_EXP_MV(
                info->Bc_s,
                info->Br_s,
                info->DB_L1_A,
                info->DB_L1_m);

            flat_attention_vector_rowsum_M_V(
                info->Bc_s,
                info->Br_s,
                info->DB_L1_A,
                info->DB_L1_l);
        }

        if (flex_is_dm_core())
        {
            if (info->slice_is_y_edge)
            {
                //Set B: < n > load K_sx
                if (valid)
                {
                    flex_dma_async_1d(
                        info->DB_L1_KT,
                        info->HBM_KT + (iter_j + (2*iter_h + 1)*info->Tc) * info->slice_KTV_size,
                        info->L1_KT_size);
                    flex_dma_async_wait_all();
                }

                //Set B: <n-1> load V_sx
                if (_valid)
                {
                    flex_dma_async_1d(
                        info->DB_L1_V,
                        info->HBM_V + (_iter_j + (2*_iter_h + 1)*info->Tc) * info->slice_KTV_size,
                        info->L1_V_size);
                    flex_dma_async_wait_all();
                }

                if (info->flatten_slice_y > 1)
                {
                    //Set B: < n > Broadcast K_sx
                    if (valid)
                    {
                        flex_dma_async_1d_broadcast(
                            remote_xy(
                                info->cluster_pos.x,
                                info->group.this_grid_top_most,
                                info->DB_L1_KT),
                            local(info->DB_L1_KT),
                            info->L1_KT_size);
                        flex_dma_async_wait_all();
                    }

                    //Set B: <n-1> Broadcast V_sx
                    if (_valid)
                    {
                        flex_dma_async_1d_broadcast(
                            remote_xy(
                                info->cluster_pos.x,
                                info->group.this_grid_top_most,
                                info->DB_L1_V),
                            local(info->DB_L1_V),
                            info->L1_V_size);
                        flex_dma_async_wait_all();
                    }
                }
            }
        }

        //Set B: <n-1> Redsum+Bcast l
        if (_valid && info->flatten_slice_x > 1)
        {
            grid_sync_group_barrier_xy(&(info->group));
            flat_attention_redsum_rowwise(
                info,
                info->DB_L1_l,
                info->L1_l_size);
            grid_sync_group_barrier_xy(&(info->group));
            flat_attention_broadcast_rowwise(
                info,
                info->DB_L1_l,
                info->L1_l_size);
            grid_sync_group_barrier_xy(&(info->group));
        }

        //Set B: <n-1> l=e*lr+l
        if (_valid && (_iter_j != 0))
        {
            if (flex_is_first_core())
            {
                flat_attention_vector_update_l(
                    info->Br_s,
                    info->DB_L1_lr,
                    info->DB_L1_e,
                    info->DB_L1_l);
            }
            flex_intra_cluster_sync();
        }
        
        //Set B: <n-1> l => lr
        if (flex_is_dm_core() && _valid)
        {
            flex_dma_async_1d(info->DB_L1_lr,info->DB_L1_l,info->L1_l_size);
            flex_dma_async_wait_all();
        }

        if (flex_is_first_core()){
            //Set B:  <n-1> O = O/e
            if (_valid && (_iter_j != 0))
            {
                flat_attention_vector_M_div_V(
                    info->d,
                    info->Br_s,
                    info->DB_L1_O,
                    info->DB_L1_e);
            }
            //Set A: < n > Att = Q*KT --- End
            if (valid)
            {
                flex_redmule_wait();
            }
        }
        flex_intra_cluster_sync();

        /********************************************************************************
         * PHASE 3: | RedMule               |   iDMA                | Spatz              |
         * ------------------------------------------------------------------------------|
         *          | Set B                 |   Set A                                    |
         *          |--------------------------------------------------------------------|
         *          |  <n-1> O = O + P*V    | <n-1> RedSum O        |                    |
         *          |                       |--------------------------------------------|
         *          |                       | <n+1> load Q_sy       | <n-1> O=O/lr       |
         *          |                       | <n+1> Broadcast Q_sy  |                    |
         *          |                       |--------------------------------------------|
         *          |                       | <n-1> store O         | < n > m=rowmax(Att)|
         *          |                       |--------------------------------------------|
         *          |                       | < n > Redmax+Bcast m  |                    |
         *          |                       |--------------------------------------------|
         *          |                       |                       | < n > e=exp(mr-m)  |
         *          |                       |                       | < n > m => mr      |
         ********************************************************************************/

        //Set B: <n-1> O = O + P*V --- Start
        if (flex_is_first_core() && _valid){
            flex_redmule_config(info->Br_s, info->Bc_s, info->d);
            flex_redmule_trigger(
                info->DB_L1_A,
                info->DB_L1_V,
                info->DB_L1_O,
                REDMULE_FP_16);
        }

        //Set A: <n-1> RedSum O
        if (flex_is_dm_core() && _valid && _iter_j == (info->Tc - 1))
        {
            if(info->cluster_pos.x == info->group.this_grid_right_most && info->flatten_slice_x > 1){
                flex_dma_async_1d_reduction(
                    remote_xy(
                        info->group.this_grid_left_most,
                        info->cluster_pos.y,
                        info->L1_O),
                    local(info->L1_O),
                    info->L1_O_size,
                    COLLECTIVE_REDADD_FP_16);
                flex_dma_async_wait_all();
            }
        }
        grid_sync_group_barrier_xy(&(info->group));

        //Set A: <n-1> O=O/lr
        if (flex_is_first_core() && _valid && _iter_j == (info->Tc - 1))
        {
            if (info->slice_is_x_edge){
                flat_attention_vector_M_div_V(
                    info->d,
                    info->Br_s,
                    info->L1_O,
                    info->L1_l);
            }
        }

        //Set A: <n+1> load Q_sy + <n+1> Broadcast Q_sy
        if (flex_is_dm_core() && valid_ && iter_j_ == 0)
        {
            if (info->slice_is_x_edge){
                flex_dma_async_1d(
                    info->L1_Q,
                    info->HBM_Q + (iter_i_ + 2*iter_h_*info->Tr) * info->slice_QO_size,
                    info->L1_Q_size);
                flex_dma_async_wait_all();

                if (info->flatten_slice_x > 1)
                {
                    flex_dma_async_1d_broadcast(
                        remote_xy(
                            info->group.this_grid_right_most,
                            info->cluster_pos.y,
                            info->L1_Q),
                        local(info->L1_Q),
                        info->L1_Q_size);
                    flex_dma_async_wait_all();
                }
            }
        }
        flex_intra_cluster_sync();

        //Set A: < n > m=rowmax(Att)
        if (flex_is_first_core() && valid)
        {
            flat_attention_vector_rowmax_MV_V(
                info->Bc_s,
                info->Br_s,
                info->L1_A,
                info->L1_mr,
                info->L1_m);
        }

        //Set A: <n-1> store O
        if (flex_is_dm_core() && _valid && _iter_j == (info->Tc - 1))
        {
            if (info->slice_is_x_edge){
                flex_dma_async_1d(
                    info->HBM_O + (_iter_i + 2*_iter_h *info->Tr) * info->slice_QO_size,
                    info->L1_O,
                    info->L1_O_size);
                flex_dma_async_wait_all();
            }

            flex_dma_async_1d(info->L1_mr,zomem(0),info->L1_m_size+info->L1_l_size);
            flex_dma_async_wait_all();

            flex_dma_async_1d(
                info->L1_O,
                zomem(0),
                info->L1_O_size);
            flex_dma_async_wait_all();
        }

        //Set A: < n > Redmax+Bcast m
        if (valid && info->flatten_slice_x > 1)
        {
            grid_sync_group_barrier_xy(&(info->group));
            flat_attention_redmax_rowwise(
                info,
                info->L1_m,
                info->L1_m_size);
            grid_sync_group_barrier_xy(&(info->group));
            flat_attention_broadcast_rowwise(
                info,
                info->L1_m,
                info->L1_m_size);
            grid_sync_group_barrier_xy(&(info->group));
        }

        //Set A: < n > e=exp(mr-m)
        if (flex_is_first_core() && valid && iter_j != 0)
        {
            flat_attention_vector_EXP_VV_V(
                info->Br_s,
                info->L1_mr,
                info->L1_m,
                info->L1_e);
        }
        flex_intra_cluster_sync();

        //Set A: < n > m => mr
        if (flex_is_dm_core() && valid)
        {
            flex_dma_async_1d(info->L1_mr,info->L1_m,info->L1_m_size);
            flex_dma_async_wait_all();
        }

        //Set B: <n-1> O = O + P*V --- End
        if (flex_is_first_core() && _valid){
            flex_redmule_wait();
        }
        flex_intra_cluster_sync();

        /*
        * Clear Set B Att Matrix
        */
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(info->DB_L1_A,zomem(0),info->L1_A_size);
            flex_dma_async_wait_all();
        }
        flex_intra_cluster_sync();

        /********************************************************************************
         * PHASE 4: | RedMule               |   iDMA                | Spatz              |
         * ------------------------------------------------------------------------------|
         *          | Set B                 |   Set A                                    |
         *          |--------------------------------------------------------------------|
         *          |  < n > Att = Q*KT     | <n+1> load K_sx       | < n > p=exp(Att-m) |
         *          |                       | < n > load V_sx       | < n > l=rowsum(p   |
         *          |                       | <n+1> Broadcast K_sx  |                    |
         *          |                       | < n > Broadcast V_sx  |                    |
         *          |                       |--------------------------------------------|
         *          |                       | < n > Redsum+Bcast l  |                    |
         *          |                       |--------------------------------------------|
         *          |                       |                       | < n > l=e*lr+l     |
         *          |                       |                       | < n > O = O/e      |
         *          |                       |                       | < n > l => lr      |
         ********************************************************************************/

        //Set B: < n > Att = Q*KT --- Start
        if (flex_is_first_core() && valid){
            flex_redmule_config(info->Br_s, info->d, info->Bc_s);
            flex_redmule_trigger(
                info->DB_L1_Q,
                info->DB_L1_KT,
                info->DB_L1_A,
                REDMULE_FP_16);
        }

        //Set A: < n > p=exp(Att-m) and < n > l=rowsum(p)
        if (flex_is_first_core() && valid){
            flat_attention_vector_EXP_MV(
                info->Bc_s,
                info->Br_s,
                info->L1_A,
                info->L1_m);

            flat_attention_vector_rowsum_M_V(
                info->Bc_s,
                info->Br_s,
                info->L1_A,
                info->L1_l);
        }

        if (flex_is_dm_core())
        {
            if (info->slice_is_y_edge)
            {
                //Set A: <n+1> load K_sx
                if (valid_)
                {
                    flex_dma_async_1d(
                        info->L1_KT,
                        info->HBM_KT + (iter_j_ + 2*iter_h_*info->Tc) * info->slice_KTV_size,
                        info->L1_KT_size);
                    flex_dma_async_wait_all();
                }

                //Set A: < n > load V_sx
                if (valid)
                {
                    flex_dma_async_1d(
                        info->L1_V,
                        info->HBM_V + (iter_j + 2*iter_h*info->Tc) * info->slice_KTV_size,
                        info->L1_V_size);
                    flex_dma_async_wait_all();
                }

                if (info->flatten_slice_y > 1)
                {
                    //Set A: <n+1> Broadcast K_sx
                    if (valid_)
                    {
                        flex_dma_async_1d_broadcast(
                            remote_xy(
                                info->cluster_pos.x,
                                info->group.this_grid_top_most,
                                info->L1_KT),
                            local(info->L1_KT),
                            info->L1_KT_size);
                        flex_dma_async_wait_all();
                    }

                    //Set A: < n > Broadcast V_sx
                    if (valid)
                    {
                        flex_dma_async_1d_broadcast(
                            remote_xy(
                                info->cluster_pos.x,
                                info->group.this_grid_top_most,
                                info->L1_V),
                            local(info->L1_V),
                            info->L1_V_size);
                        flex_dma_async_wait_all();
                    }
                }
            }
        }

        //Set A: < n > Redsum+Bcast l
        if (valid && info->flatten_slice_x > 1)
        {
            grid_sync_group_barrier_xy(&(info->group));
            flat_attention_redsum_rowwise(
                info,
                info->L1_l,
                info->L1_l_size);
            grid_sync_group_barrier_xy(&(info->group));
            flat_attention_broadcast_rowwise(
                info,
                info->L1_l,
                info->L1_l_size);
            grid_sync_group_barrier_xy(&(info->group));
        }

        //Set A: < n > l=e*lr+l
        if (valid && (iter_j != 0))
        {
            if (flex_is_first_core())
            {
                flat_attention_vector_update_l(
                    info->Br_s,
                    info->L1_lr,
                    info->L1_e,
                    info->L1_l);
            }
            flex_intra_cluster_sync();
        }
        
        //Set A: < n > l => lr
        if (flex_is_dm_core() && valid)
        {
            flex_dma_async_1d(info->L1_lr,info->L1_l,info->L1_l_size);
            flex_dma_async_wait_all();
        }

        if (flex_is_first_core()){
            //Set A:  < n > O = O/e
            if (valid && (iter_j != 0))
            {
                flat_attention_vector_M_div_V(
                    info->d,
                    info->Br_s,
                    info->L1_O,
                    info->L1_e);
            }
            //Set B: < n > Att = Q*KT --- End
            if (valid)
            {
                flex_redmule_wait();
            }
        }
        flex_intra_cluster_sync();

        /*
        * Update iteration
        */
        attn_iter_ = ai_get_next(&attn_iter_);
        __valid = _valid;
        _valid = valid;
        valid = valid_;
        valid_ = ai_is_valid(attn_iter_);

        __iter_j = _iter_j;
        _iter_j = iter_j;
        iter_j = iter_j_;
        iter_j_ = attn_iter_.j;

        __iter_i = _iter_i;
        _iter_i = iter_i;
        iter_i = iter_i_;
        iter_i_ = attn_iter_.i;

        __iter_h = _iter_h;
        _iter_h = iter_h;
        iter_h = iter_h_;
        iter_h_ = attn_iter_.h;
    }
}

#endif