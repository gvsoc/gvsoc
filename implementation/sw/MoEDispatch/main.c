// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 1.Oct.2025

#include "flex_runtime.h"
#include "flex_dump.h"
#include "moed.h"
#include "preload.h"
#include "MoEDispatch.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    MoEDispatchInfo info = MoEDispatchAnaylze(
        MOED_NUM_TOKENS/*num_total_token*/,
        MOED_EMBEDDED_LENGTH/*embedded_length*/,
        MOED_NUM_ROUTED_EXPERTS/*num_routed_experts*/,
        MOED_NUM_ACTIVE_EXPERTS/*num_active_experts*/,
        MOED_TOKEN_PER_CLUSTER/*token_per_cluster*/,
        I_ADDR/*input_address*/,
        S_ADDR/*output_address*/,
        D_ADDR/*idx_address*/,
        P_ADDR/*pos_address*/);
    flex_global_barrier_xy();

    //execute MoEGate
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    MoEDispatchRun(&info);
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

    //dump results
    if (MOED_MOED_NUMER)
    {
        //postload part of O
        if (flex_get_cluster_id() == 0 && flex_is_dm_core())
        {
            //1. Calculate the whole size of the output
            uint64_t output_size = MOED_NUM_TOKENS * MOED_NUM_ACTIVE_EXPERTS * sizeof(int);
            uint64_t num_output_chunk = (output_size + MOED_MOED_NUMER_CHUNK - 1) / MOED_MOED_NUMER_CHUNK;

            //2. iterate over chunks
            flex_dump_open();
            for (uint64_t i = 0; i < num_output_chunk; ++i)
            {
                flex_dump_hbm(i * MOED_MOED_NUMER_CHUNK + D_ADDR - ARCH_HBM_START_BASE, MOED_MOED_NUMER_CHUNK);
                flex_dump_hbm(i * MOED_MOED_NUMER_CHUNK + P_ADDR - ARCH_HBM_START_BASE, MOED_MOED_NUMER_CHUNK);
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