// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 1.Oct.2025

#include "flex_runtime.h"
#include "attn.h"
#include "preload.h"
#include "FlatAttention.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    flat_attention(
        ATTN_KV_SEQUENCE_LENGTH/*kv_sequence_length*/,
        ATTN_Q_SEQUENCE_LENGTH/*q_sequence_length*/,
        ATTN_SPECULATIVE_LENGTH/*speculative_length*/,
        ATTN_HEAD_DIMEMSION/*head_dimemsion*/,
        ATTN_NUM_HEAD/*num_head*/,
        ATTN_NUM_HEAD_GROUP/*num_head_group*/,
        ATTN_BATCH_SIZE/*batch_size*/,
        ATTN_FLATTEN_SCALE_X/*flatten_scale_x*/,
        ATTN_FLATTEN_SCALE_Y/*flatten_scale_y*/,
        ATTN_FLATTEN_SHAPE_X/*flatten_shape_x*/,
        ATTN_FLATTEN_SHAPE_Y/*flatten_shape_y*/,
        Q_ADDR/*Q_base_address*/,
        K_ADDR/*K_base_address*/,
        V_ADDR/*V_base_address*/,
        O_EADDR/*O_base_address*/,
        O_GADDR/*O_golden_base_address*/,
        ATTN_FLATTEN_ASYNC/*async_enable*/,
        0/*dump_enable*/);

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}