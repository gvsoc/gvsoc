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

// input output type define
// value aligned with byte-width
// TYPE (1): fp8
// TYPE (2): fp16
// TYPE (4): fp32
#ifndef _FLEX_TYPE
#define _FLEX_TYPE
#define _IN_TYPE  (1)
#define _OUT_TYPE (2)
#endif

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
        // allocation
        uint8_t *matrix_a = (uint8_t *)flex_l1_malloc(FP8_M * FP8_N * sizeof(uint8_t));
        uint8_t *matrix_b = (uint8_t *)flex_l1_malloc(FP8_N * FP8_P * sizeof(uint8_t));
        #if _OUT_TYPE == 1
        uint8_t *matrix_c = (uint8_t *)flex_l1_malloc(FP8_M * FP8_P * sizeof(uint8_t));
        #elif _OUT_TYPE == 2
        uint16_t *matrix_c = (uint16_t *)flex_l1_malloc(FP8_M * FP8_P * sizeof(uint16_t));
        #else
            #error "Unsupported _OUT_TYPE value"
        #endif

        // data movement
        for (uint32_t i=0; i<FP8_M * FP8_N; i++){
            matrix_a[i] = matrix_a_fp8[i];
        }
        for (uint32_t i=0; i<FP8_N * FP8_P; i++){
            matrix_b[i] = matrix_b_fp8[i];
        }
        #if _OUT_TYPE == 1
        spatz_matmul_fp8(matrix_a, matrix_b, matrix_c, FP8_M, FP8_N, FP8_P);
        // verify
        spatz_verify(FP8_M * FP8_P, matrix_c, matrix_c_fp8, 0.25f);
        #elif _OUT_TYPE == 2
        spatz_matmul_fp16(matrix_a, matrix_b, matrix_c, FP8_M, FP8_N, FP8_P);
        // verify
        spatz_verify_16(FP8_M * FP8_P, matrix_c, matrix_c_fp16, 0.25f);
        #endif

        
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}