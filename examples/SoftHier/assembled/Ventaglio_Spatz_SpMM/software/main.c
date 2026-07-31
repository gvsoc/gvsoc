// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// N:M SpMM on SoftHier using Spatz + Ventaglio.
// Supports fp16 and fp32 (set by data header's DATA_TYPE_WIDTH).

#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"
#include "flex_runtime.h"

#include "data_spmm_fp32.h"  // defines DATA_TYPE_WIDTH, SPMM_M/K/N/PW, arrays
#include "sp_fspmm.h"
#include <math.h>

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    flex_alloc_init();
    flex_intra_cluster_sync();
    flex_global_barrier_xy();
    flex_intra_cluster_sync();

    uint32_t CID     = flex_get_cluster_id();
    uint32_t core_id = flex_get_core_id();

    if (core_id == 0 && CID == 0) {

        spmm_data_t *a   = (spmm_data_t *)flex_l1_malloc(SPMM_M * SPMM_K * sizeof(spmm_data_t));
        spmm_data_t *w   = (spmm_data_t *)flex_l1_malloc(SPMM_K * SPMM_PW * sizeof(spmm_data_t));
        uint32_t    *idx = (uint32_t *)flex_l1_malloc(SPMM_K * SPMM_PW * sizeof(uint32_t));
        spmm_data_t *c   = (spmm_data_t *)flex_l1_malloc(SPMM_M * SPMM_N * sizeof(spmm_data_t));

#if DATA_TYPE_WIDTH == 16
        const uint16_t *src_a = matrix_a_fp32, *src_w = matrix_w_compact;
        uint16_t *dst_a = (uint16_t *)a, *dst_w = (uint16_t *)w, *dst_c = (uint16_t *)c;
#else
        const uint32_t *src_a = matrix_a_fp32, *src_w = matrix_w_compact;
        uint32_t *dst_a = (uint32_t *)a, *dst_w = (uint32_t *)w, *dst_c = (uint32_t *)c;
#endif
        const uint32_t *src_idx = matrix_idx;
        uint32_t *dst_idx = (uint32_t *)idx;

        for (uint32_t i = 0; i < SPMM_M * SPMM_K; i++) dst_a[i] = src_a[i];
        for (uint32_t i = 0; i < SPMM_K * SPMM_PW; i++) dst_w[i] = src_w[i];
        for (uint32_t i = 0; i < SPMM_K * SPMM_PW; i++) dst_idx[i] = src_idx[i];
        for (uint32_t i = 0; i < SPMM_M * SPMM_N; i++) dst_c[i] = 0;

        flex_timer_start();
        spmm_4xVL(c, a, w, idx, 0, SPMM_M, SPMM_K, SPMM_N, SPMM_PW, 0, SPMM_PW);
        flex_timer_end();

        printf("[DONE] %s SpMM (%dx%dx%d, %d:%d sparse, P_W=%d)\n",
               (DATA_TYPE_WIDTH == 16) ? "fp16" : "fp32",
               SPMM_M, SPMM_K, SPMM_N, SPMM_N_SPARSE, SPMM_M_SPARSE, SPMM_PW);
    }

    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}
