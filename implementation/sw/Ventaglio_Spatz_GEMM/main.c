// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// SUMMA GEMM using Spatz vector processor for local tile computation.
// Drop-in replacement for SummaGEMM (RedMule) in the E2E pipeline.

#include "flex_runtime.h"
#include "flex_dump.h"
#include "spatz_gemm.h"
#include "preload.h"
#include "SummaSpatzGEMM.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();

    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    SummaGEMMInfo info = SummaGEMMAnaylze(
        X_ADDR,
        W_ADDR,
        Z_EADDR,
        GEMM_M_SIZE,
        GEMM_N_SIZE,
        GEMM_K_SIZE,
        GEMM_M_TILE,
        GEMM_N_TILE,
        GEMM_K_TILE,
        GEMM_SUMMA_SCALE_X,
        GEMM_SUMMA_SCALE_Y,
        GEMM_SUMMA_GROUP_NUMBER,
        GEMM_SUMMA_GROUP_REDUCE,
        GEMM_SUMMA_GROUP_SPLITK,
        GEMM_SUMMA_GROUP_SPLITN,
        GEMM_SUMMA_GROUP_GAP_X,
        GEMM_SUMMA_GROUP_GAP_W,
        GEMM_SUMMA_GROUP_GAP_Z);
    flex_global_barrier_xy();

    // Execute SUMMA GEMM with Spatz local compute
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    SummaGEMMRun(&info);
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}
