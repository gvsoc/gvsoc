// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 1.Oct.2025

#include "flex_runtime.h"
#include "flex_dump.h"
#include "moec.h"
#include "preload.h"
#include "MoECombine.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    MoECombineInfo info = MoECombineAnaylze(
        MOEC_NUM_TOKENS/*num_total_token*/,
        MOEC_EMBEDDED_LENGTH/*embedded_length*/,
        MOEC_NUM_ROUTED_EXPERTS/*num_routed_experts*/,
        MOEC_NUM_ACTIVE_EXPERTS/*num_active_experts*/,
        MOEC_TOKEN_PER_CLUSTER/*token_per_cluster*/,
        I_ADDR/*input_address*/,
        O_EADDR/*output_address*/,
        V_ADDR/*val_address*/,
        D_ADDR/*idx_address*/,
        P_ADDR/*pos_address*/);
    flex_global_barrier_xy();

    //execute MoEGate
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    MoECombineRun(&info);
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

    //dump results
    if (MOEC_MOEC_NUMER)
    {
        //postload part of O
        if (flex_get_cluster_id() == 0 && flex_is_dm_core())
        {
            //1. Calculate the whole size of the output
            uint64_t output_size = MOEC_NUM_TOKENS * MOEC_EMBEDDED_LENGTH * DATA_TYPE_BYTE;
            uint64_t num_output_chunk = (output_size + MOEC_MOEC_NUMER_CHUNK - 1) / MOEC_MOEC_NUMER_CHUNK;

            //2. iterate over chunks
            flex_dump_open();
            for (uint64_t i = 0; i < num_output_chunk; ++i)
            {
                flex_dump_hbm(i * MOEC_MOEC_NUMER_CHUNK + O_EADDR - ARCH_HBM_START_BASE, MOEC_MOEC_NUMER_CHUNK);
                flex_dump_hbm(i * MOEC_MOEC_NUMER_CHUNK + O_GADDR - ARCH_HBM_START_BASE, MOEC_MOEC_NUMER_CHUNK);
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