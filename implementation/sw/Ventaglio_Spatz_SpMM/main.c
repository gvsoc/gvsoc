// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// SUMMA SpMM using Spatz + Ventaglio for local sparse tile computation.

#include "flex_runtime.h"
#include "flex_dump.h"
#include "spatz_spmm.h"
#include "preload.h"
#include "SummaSpatzSpMM.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();

    SummaGEMMInfo info = SummaGEMMAnaylze(
        X_ADDR, W_ADDR, Z_EADDR,
        GEMM_M_SIZE, GEMM_N_SIZE, GEMM_K_SIZE,
        GEMM_M_TILE, GEMM_N_TILE, GEMM_K_TILE,
        GEMM_SUMMA_SCALE_X, GEMM_SUMMA_SCALE_Y,
        GEMM_SUMMA_GROUP_NUMBER, GEMM_SUMMA_GROUP_REDUCE,
        GEMM_SUMMA_GROUP_SPLITK, GEMM_SUMMA_GROUP_SPLITN,
        GEMM_SUMMA_GROUP_GAP_X, GEMM_SUMMA_GROUP_GAP_W, GEMM_SUMMA_GROUP_GAP_Z);
    flex_global_barrier_xy();

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    SummaGEMMRun(&info);
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();

    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}
