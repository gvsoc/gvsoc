// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 1.Oct.2025

#include "flex_runtime.h"
#include "flex_dump.h"
#include "tmla.h"
#include "preload.h"
#include "FlatMLA.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    FlatMLAInfo info = FlatMLA_analyze(
        QN_ADDR/*Q_NOPE_address*/,
        QR_ADDR/*Q_ROPE_address*/,
        CN_ADDR/*C_NOPE_address*/,
        CR_ADDR/*C_ROPE_address*/,
        O_EADDR/*O_address*/,
        TMLA_KV_SEQUENCE_LENGTH/*kv_sequence_length*/,
        TMLA_Q_SEQUENCE_LENGTH/*q_sequence_length*/,
        TMLA_SPECULATIVE_LENGTH/*speculative_length*/,
        TMLA_NOPE_HEAD_DIM/*nope_head_dim*/,
        TMLA_ROPE_HEAD_DIM/*rope_head_dim*/,
        TMLA_NUM_HEAD/*num_head*/,
        TMLA_BATCH_SIZE/*batch_size*/,
        TMLA_FLATTEN_SCALE_X/*flatten_scale_x*/,
        TMLA_FLATTEN_SCALE_Y/*flatten_scale_y*/,
        TMLA_FLATTEN_SHAPE_X/*flatten_shape_x*/,
        TMLA_FLATTEN_SHAPE_Y/*flatten_shape_y*/);
    flex_global_barrier_xy();

    //execute Activation
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    if (TMLA_FLATTEN_ASYNC)
    {
        if (info.work_group_enable) FlatMLA_ayncrun(&info);
    } else {
        if (info.work_group_enable) FlatMLA_run(&info);
    }
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

    //dump results
    if (TMLA_FLATTEN_NUMER)
    {
        //postload part of O
        if (flex_get_cluster_id() == 0 && flex_is_dm_core())
        {
            //1. Calculate the whole size of the output
            uint64_t output_size = TMLA_Q_SEQUENCE_LENGTH * TMLA_SPECULATIVE_LENGTH * TMLA_NUM_HEAD * TMLA_NOPE_HEAD_DIM * DATA_TYPE_BYTE;
            uint64_t num_output_chunk = (output_size + TMLA_FLATTEN_NUMER_CHUNK - 1) / TMLA_FLATTEN_NUMER_CHUNK;

            //2. iterate over chunks
            flex_dump_open();
            for (uint64_t i = 0; i < num_output_chunk; ++i)
            {
                flex_dump_hbm(i * TMLA_FLATTEN_NUMER_CHUNK + O_EADDR - ARCH_HBM_START_BASE, TMLA_FLATTEN_NUMER_CHUNK);
                flex_dump_hbm(i * TMLA_FLATTEN_NUMER_CHUNK + O_GADDR - ARCH_HBM_START_BASE, TMLA_FLATTEN_NUMER_CHUNK);
            }
            flex_dump_close();
        }
    }
    flex_global_barrier_xy();

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}