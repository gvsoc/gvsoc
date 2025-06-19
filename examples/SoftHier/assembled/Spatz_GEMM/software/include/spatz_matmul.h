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

// Spatz MatMul library: A x B = C, (MxN) x (NxP) --> (MxP)
//
//
// spatz_matmul_fp8():  dense matmul [Gustav], input _fp8, output _fp8
// spatz_matmul_fp16(): dense matmul [Gustav], input _fp8, output _fp16

#ifndef _SPATZ_GEMM_H_
#define _SPATZ_GEMM_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_libfp8.h"

#pragma GCC optimize("no-tree-loop-distribute-patterns")

// dense matmul [Gustav], input _fp8, output _fp8
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

// dense matmul [Gustav], input _fp8, output _fp16
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

// sparse matmul [Gustav], AspB, input _fp8, index _uint8, output _fp16
// effective output elements are gather/scatter with VLSU
void spatz_AspB_matmul_fp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, uint8_t* index_b,
                  const uint32_t M, const uint32_t N, const uint32_t P, 
                  const uint8_t spN, const uint8_t spM){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P * spN / spM;
    uint32_t vl;
    uint32_t pre_vl = 0;
    do{
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
        if (pre_vl != vl){    // vl changed, index re-calculate, otherwise skip
            for (uint32_t i=0; i<N; i++){                        // row
                for (uint32_t j=0; j<((P*spN/spM)/vl); j++){     // vl block
                    for (uint32_t k=0; k<vl/spN; k++){           // nm block
                        for (uint32_t q=0; q<spN; q++) index_b[q + k*spN + j*vl + i*(P*spN/spM)] += k*spM;
                    }
                }
            }
            pre_vl = vl;
        }
        
        for (uint32_t m = 0; m < M; m++){
            uint16_t *p_c = matrix_c + m*P + p;
            for (uint32_t n = 0; n < N; n++){
                // load a
                uint8_t *p_a = &matrix_a[m*N + n];
                asm volatile("flw fa0, (%0)" :: "r"(p_a));

                // load b vec
                uint8_t *p_b = &matrix_b[p + n*P*spN/spM];
                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));

                // load index 
                uint8_t *p_index = &index_b[p + n*P*spN/spM];
                asm volatile("vle8.v v8, (%0)" ::"r"(p_index));

                if (n==0){ // first iter
                    asm volatile("vmv.v.i v16, 0");
                    asm volatile("vfwmul.vf v16, v0, fa0");
                    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                    asm volatile("vsuxei8.v v16, (%0), v8" ::"r"(p_c));
                    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
                } else {
                    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                    asm volatile("vluxei8.v v16, (%0), v8" ::"r"(p_c));
                    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
                    asm volatile("vfwmacc.vf v16, fa0, v0");
                    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(avl));
                    asm volatile("vsuxei8.v v16, (%0), v8" ::"r"(p_c));
                    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
                }
            }
        }
        avl -= vl;
        p += vl;
    } while (avl>0);
}

#endif