// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 6.Jan.2025

#include "flex_runtime.h"
#include "FlatAttentionConfig.h"
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
        ATTN_SEQUENCE_LENGTH/*sequence_length*/,
        ATTN_HEAD_DIMEMSION/*head_dimemsion*/,
        ATTN_NUM_HEAD/*head_num*/,
        ATTN_BATCH_SIZE/*batch_size*/,
        ATTN_ELEM_SIZE/*elem_size*/,
        ATTN_FLATTEN_SCALE/*flatten_scale*/,
        ATTN_FLATTEN_SHAPE/*flatten_shape*/,
        ATTN_FLATTEN_ASYNC/*async_enable*/,
        ATTN_FLATTEN_NUMERICAL_CHECK/*dump_enable*/);

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}