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
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Bowen Wang, ETH Zurich

// Dense MatMul: A x B = C, (MxN) x (NxP) --> (MxP)
// Data Width:   8-bit

#ifndef _SPATZ_GEMM_H_
#define _SPATZ_GEMM_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_libfp8.h"

#pragma GCC optimize("no-tree-loop-distribute-patterns")

// outer product 
void spatz_matmul_fp8(uint8_t* matrix_a, uint8_t* matrix_b, uint8_t* matrix_c, 
                  const uint32_t M, const uint32_t N, const uint32_t P){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P;
    uint32_t vl;
    do{
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
        for (uint32_t m = 0; m < M; m++){
            uint8_t *p_c = matrix_c + m*P + p;
            for (uint32_t n = 0; n < N; n++){
                // load a
                uint8_t *p_a = &matrix_a[m*N + n];
                asm volatile("flw fa0, (%0)" :: "r"(p_a));

                // load b vec
                uint8_t *p_b = &matrix_b[p + n*P];

                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));
                if (n==0){ // first iter
                    asm volatile("vmv.v.i v16, 0");
                    asm volatile("vfmul.vf v16, v0, fa0");
                } else {
                    asm volatile("vfmacc.vf v16, fa0, v0");
                }
            }
            asm volatile("vse8.v v16, (%0)" ::"r"(p_c));
        }
        avl -= vl;
        p += vl;
    } while (avl>0);
}

void spatz_matmul_fp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, 
                  const uint32_t M, const uint32_t N, const uint32_t P){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P;
    uint32_t vl;
    do{
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
        for (uint32_t m = 0; m < M; m++){
            uint16_t *p_c = matrix_c + m*P + p;
            for (uint32_t n = 0; n < N; n++){
                // load a
                uint8_t *p_a = &matrix_a[m*N + n];
                asm volatile("flw fa0, (%0)" :: "r"(p_a));

                // load b vec
                uint8_t *p_b = &matrix_b[p + n*P];

                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));
                if (n==0){ // first iter
                    asm volatile("vmv.v.i v16, 0");
                    asm volatile("vfwmul.vf v16, v0, fa0");
                } else {
                    asm volatile("vfwmacc.vf v16, fa0, v0");
                }
            }
            asm volatile("vse16.v v16, (%0)" ::"r"(p_c));
        }
        avl -= vl;
        p += vl;
    } while (avl>0);

}

#endif