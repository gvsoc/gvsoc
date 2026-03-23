// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// Dense GEMM on SoftHier using Spatz vector processor.
// Supports fp16 and fp32 (set by data header's DATA_TYPE_WIDTH).

#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"
#include "flex_runtime.h"

#include "data_fp32.h"  // defines DATA_TYPE_WIDTH, GEMM_M/K/N, matrix arrays
#include "sp_fmatmul.h"
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

        gemm_data_t *a = (gemm_data_t *)flex_l1_malloc(GEMM_M * GEMM_K * sizeof(gemm_data_t));
        gemm_data_t *b = (gemm_data_t *)flex_l1_malloc(GEMM_K * GEMM_N * sizeof(gemm_data_t));
        gemm_data_t *c = (gemm_data_t *)flex_l1_malloc(GEMM_M * GEMM_N * sizeof(gemm_data_t));

        // Copy data (works for both uint16_t and uint32_t element types)
#if DATA_TYPE_WIDTH == 16
        uint16_t *dst_a = (uint16_t *)a, *dst_b = (uint16_t *)b, *dst_c = (uint16_t *)c;
        const uint16_t *src_a = matrix_a_fp32, *src_b = matrix_b_fp32;
#else
        uint32_t *dst_a = (uint32_t *)a, *dst_b = (uint32_t *)b, *dst_c = (uint32_t *)c;
        const uint32_t *src_a = matrix_a_fp32, *src_b = matrix_b_fp32;
#endif
        for (uint32_t i = 0; i < GEMM_M * GEMM_K; i++) dst_a[i] = src_a[i];
        for (uint32_t i = 0; i < GEMM_K * GEMM_N; i++) dst_b[i] = src_b[i];
        for (uint32_t i = 0; i < GEMM_M * GEMM_N; i++) dst_c[i] = 0;

        flex_timer_start();
        matmul_4xVL(c, a, b, 0, GEMM_M, GEMM_K, GEMM_N, 0, GEMM_N);
        flex_timer_end();

        printf("[PASS] %s GEMM (%dx%dx%d) completed\n",
               (DATA_TYPE_WIDTH == 16) ? "fp16" : "fp32", GEMM_M, GEMM_K, GEMM_N);
    }

    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}
