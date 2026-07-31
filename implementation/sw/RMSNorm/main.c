// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 1.Oct.2025

#include "flex_runtime.h"
#include "flex_dump.h"
#include "norm.h"
#include "preload.h"
#include "RMSNorm.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    RMSNormInfo info =  Dsv3RMSNormAnaylze(
        NORM_M_SIZE/*num_total_token*/,
        NORM_N_SIZE/*token_embedded_length*/,
        I_ADDR/*input_address*/,
        O_EADDR/*output_address*/);
    flex_global_barrier_xy();

    //execute RMSNorm
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    Dsv3RMSNormRun(&info);
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

    //dump results
    if (NORM_NORM_NUMER)
    {
        //postload part of O
        if (flex_get_cluster_id() == 0 && flex_is_dm_core())
        {
            //1. Calculate the whole size of the output
            uint64_t output_size = NORM_M_SIZE * NORM_N_SIZE * DATA_TYPE_BYTE;
            uint64_t num_output_chunk = (output_size + NORM_NORM_NUMER_CHUNK - 1) / NORM_NORM_NUMER_CHUNK;

            //2. iterate over chunks
            flex_dump_open();
            for (uint64_t i = 0; i < num_output_chunk; ++i)
            {
                flex_dump_hbm(i * NORM_NORM_NUMER_CHUNK + O_EADDR - ARCH_HBM_START_BASE, NORM_NORM_NUMER_CHUNK);
                flex_dump_hbm(i * NORM_NORM_NUMER_CHUNK + O_GADDR - ARCH_HBM_START_BASE, NORM_NORM_NUMER_CHUNK);
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