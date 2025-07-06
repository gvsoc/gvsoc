// Copyright 2025 ETH Zurich and University of Bologna.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Author: Bowen Wang, ETH Zurich
// <main.c> for matmul

#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"
#include "flex_runtime.h"

#include "spatz_matmul.h"
#include "data_spatz_matmul_fp8.h"
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

    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    uint32_t CID = flex_get_cluster_id(); // Get cluster ID
    uint32_t core_id = flex_get_core_id();

    if (core_id == 0 && CID == 0){
        // Step1: allocation
        uint8_t *matrix_a = (uint8_t *)flex_l1_malloc(FP8_M * FP8_N * sizeof(uint8_t));
        #ifndef __SPARSE__
        uint8_t *matrix_b = (uint8_t *)flex_l1_malloc(FP8_N * FP8_P * sizeof(uint8_t));
        #else
        uint8_t *matrix_b = (uint8_t *)flex_l1_malloc(FP8_N * FP8_P * spN/spM * sizeof(uint8_t));
        #if _IDX_TYPE == _IDX_COMPACT
        uint8_t *index_b  = (uint8_t *)flex_l1_malloc(FP8_N * FP8_P * spN/spM * sizeof(uint8_t) / _IDX_PER_BYTE);
        #else
        uint8_t *index_b  = (uint8_t *)flex_l1_malloc(FP8_N * FP8_P * spN/spM * sizeof(uint8_t));
        #endif // _IDX_TYPE == _IDX_COMPACT
        #endif // __SPARSE__ allocation

        #if _OUT_TYPE == 1
        uint8_t *matrix_c = (uint8_t *)flex_l1_malloc(FP8_M * FP8_P * sizeof(uint8_t));
        #elif _OUT_TYPE == 2
        uint16_t *matrix_c = (uint16_t *)flex_l1_malloc(FP8_M * FP8_P * sizeof(uint16_t));
        #else
            #error "Unsupported _OUT_TYPE value"
        #endif // _OUT_TYPE

        // Step 2: data movement
        for (uint32_t i=0; i<FP8_M * FP8_N; i++){
            matrix_a[i] = matrix_a_fp8[i];
        }
        #ifndef __SPARSE__
        for (uint32_t i=0; i<FP8_N * FP8_P; i++){
            matrix_b[i] = matrix_b_fp8[i];
        }
        #else

        #if _IDX_TYPE == _IDX_COMPACT
        for (uint32_t i=0; i<FP8_N * FP8_P * spN/spM; i++){
            matrix_b[i] = matrix_b_compact_fp8[i];
        }
        for (uint32_t i=0; i<FP8_N * FP8_P * spN/spM / _IDX_PER_BYTE; i++){
            // less movement required for compact indices
            index_b[i] = matrix_b_index_compact_uint8[i];
        }
        #else
        for (uint32_t i=0; i<FP8_N * FP8_P * spN/spM; i++){
            matrix_b[i] = matrix_b_compact_fp8[i];    
            index_b[i] = matrix_b_index_uint8[i];
        }
        #endif // _IDX_TYPE == _IDX_COMPACT

        for (uint32_t i=0; i<FP8_M * FP8_P; i++){
            matrix_c[i] = 0; // init matrix c
        }
        #endif // __SPARSE__ data movement

        // Step 3: Compute
        flex_timer_start();
        #if _OUT_TYPE == 1
        spatz_matmul_fp8(matrix_a, matrix_b, matrix_c, FP8_M, FP8_N, FP8_P);
        // verify
        spatz_verify(FP8_M * FP8_P, matrix_c, matrix_c_fp8, 0.25f);
        #elif _OUT_TYPE == 2

        #ifndef __SPARSE__
        spatz_matmul_fp16(matrix_a, matrix_b, matrix_c, FP8_M, FP8_N, FP8_P);
        #else

        #if _IDX_TYPE == _IDX_COMPACT
        spatz_AspB_matmul_wxfp16(matrix_a, matrix_b, matrix_c, index_b, FP8_M, FP8_N, FP8_P, spN, spM);
        #else
        spatz_AspB_matmul_fp16(matrix_a, matrix_b, matrix_c, index_b, FP8_M, FP8_N, FP8_P, spN, spM);
        #endif
        #endif // __SPARSE__ compute
        flex_timer_end();

        // verify
        spatz_verify_16(FP8_M * FP8_P, matrix_c, matrix_c_fp16, 0.25f);
        #endif // _OUT_TYPT compute
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}