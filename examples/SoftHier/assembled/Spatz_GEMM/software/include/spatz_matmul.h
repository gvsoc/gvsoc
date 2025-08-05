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

// sparse matmul [Gustav], AspB, input _fp8, index _uint2, output _fp16
// effective output elements are gather/scatter with VLSU
#define IDX_PER_BYTE (4)
void spatz_AspB_matmul_wxfp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, uint8_t* index_b,
                  const uint32_t M, const uint32_t N, const uint32_t P, 
                  const uint8_t spN, const uint8_t spM){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P * spN / spM;
    uint32_t vl, vl_dump;

    do{ // outer loop 
        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
        for (uint32_t m = 0; m < M; m++){ // mid loop
            uint16_t *p_c = matrix_c + m*P + p;
            for (uint32_t n = 0; n < N; n++){
                // load a
                uint8_t *p_a = &matrix_a[m*N + n];
                asm volatile("flw ft0, (%0)" :: "r"(p_a));

                // load b vec
                // TODO: adapt to other NM formats
                // uint8_t *p_b = &matrix_b[p + n*P*spN/spM];
                uint8_t *p_b = &matrix_b[p + n*P/2];

                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));

                // load index 
                // TODO: adapt to other formats
                // uint8_t *p_index = &index_b[(p + n*P*spN/spM)/IDX_PER_BYTE];
                uint8_t *p_index = &index_b[(p + n*P/2)/IDX_PER_BYTE];
                asm volatile("vle8.v v8, (%0)" ::"r"(p_index));

                if (n==0){ // first iter
                    // reset registers
                    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl_dump) : "r"(2*avl));
                    asm volatile("vmv.v.i v16, 0");
                    asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
                    // vfwxmul.vf _inp, _idx, _fp, _oup
                    asm volatile(
                        ".word  (0b110011  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b01000   << 15) | \
                                (0b000     << 12) | \
                                (0b10000   <<  7) | \
                                (0b1010110 <<  0)   \n");
                    
                } else {
                    // vfwxmacc.vf _inp, _idx, _fp, _oup
                    asm volatile(
                        ".word  (0b110001  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b01000   << 15) | \
                                (0b000     << 12) | \
                                (0b10000   <<  7) | \
                                (0b1010110 <<  0)   \n");
                }
                // flex_timer_end();
            }
            asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl_dump) : "r"(avl*2));
            asm volatile("vse16.v v16, (%0)" ::"r"(p_c));
            asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(avl));
        }
        avl -= vl;
        p += vl;
    } while (avl>0);
}

// Unroll-optimized kernels
// 2x-unrolled dense matmul [Gustav], input _fp8, output _fp16
void spatz_matmul_unroll2_fp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, 
                  const uint32_t M, const uint32_t N, const uint32_t P){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P;
    uint32_t vl;
    do{
        asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
        for (uint32_t m = 0; m < M; m+=2){
            uint16_t *p_c = matrix_c + m*P + p;
            uint16_t *p_c2 = matrix_c + m*P + p + P;
            for (uint32_t n = 0; n < N; n++){
                // load a
                uint8_t *p_a  = &matrix_a[m*N + n];
                uint8_t *p_a2 = &matrix_a[m*N + n + N];
                asm volatile("flw fa0, (%0)" :: "r"(p_a));
                asm volatile("flw fa1, (%0)" :: "r"(p_a2));

                // load b vec
                uint8_t *p_b = &matrix_b[p + n*P];

                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));
                if (n==0){ // first iter
                    asm volatile("vmv.v.i v16, 0");
                    asm volatile("vmv.v.i v20, 0");
                    asm volatile("vfwmul.vf v16, v0, fa0");
                    asm volatile("vfwmul.vf v20, v0, fa1");
                } else {
                    asm volatile("vfwmacc.vf v16, fa0, v0");
                    asm volatile("vfwmacc.vf v20, fa1, v0");
                }
            }
            asm volatile("vse16.v v16, (%0)" ::"r"(p_c));
            asm volatile("vse16.v v20, (%0)" ::"r"(p_c2));
        }
        avl -= vl;
        p += vl;
    } while (avl>0);
}

// 4x-unrolled dense matmul [Gustav], input _fp8, output _fp16
void spatz_matmul_unroll4_fp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, 
                  const uint32_t M, const uint32_t N, const uint32_t P){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P;
    uint32_t vl;
    do{
        asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        for (uint32_t m = 0; m < M; m+=4){
            uint16_t *p_c  = matrix_c + m*P + p;
            uint16_t *p_c2 = matrix_c + m*P + p + P;
            uint16_t *p_c3 = matrix_c + m*P + p + 2*P;
            uint16_t *p_c4 = matrix_c + m*P + p + 3*P;
            for (uint32_t n = 0; n < N; n++){
                // load a
                uint8_t *p_a  = &matrix_a[m*N + n];
                uint8_t *p_a2 = &matrix_a[m*N + n + N];
                uint8_t *p_a3 = &matrix_a[m*N + n + 2*N];
                uint8_t *p_a4 = &matrix_a[m*N + n + 3*N];
                asm volatile("flw fa0, (%0)" :: "r"(p_a));
                asm volatile("flw fa1, (%0)" :: "r"(p_a2));
                asm volatile("flw fa2, (%0)" :: "r"(p_a3));
                asm volatile("flw fa3, (%0)" :: "r"(p_a4));

                // load b vec
                uint8_t *p_b = &matrix_b[p + n*P];

                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));
                if (n==0){ // first iter
                    asm volatile("vmv.v.i v8, 0");
                    asm volatile("vmv.v.i v12, 0");
                    asm volatile("vmv.v.i v16, 0");
                    asm volatile("vmv.v.i v20, 0");
                    asm volatile("vfwmul.vf v8, v0, fa0");
                    asm volatile("vfwmul.vf v12, v0, fa1");
                    asm volatile("vfwmul.vf v16, v0, fa2");
                    asm volatile("vfwmul.vf v20, v0, fa3");
                } else {
                    asm volatile("vfwmacc.vf v8, fa0, v0");
                    asm volatile("vfwmacc.vf v12, fa1, v0");
                    asm volatile("vfwmacc.vf v16, fa2, v0");
                    asm volatile("vfwmacc.vf v20, fa3, v0");
                }
            }
            asm volatile("vse16.v v8,  (%0)" ::"r"(p_c));
            asm volatile("vse16.v v12, (%0)" ::"r"(p_c2));
            asm volatile("vse16.v v16, (%0)" ::"r"(p_c3));
            asm volatile("vse16.v v20, (%0)" ::"r"(p_c4));
        }
        avl -= vl;
        p += vl;
    } while (avl>0);
}


void spatz_AspB_matmul_unroll4_wxfp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, uint8_t* index_b,
                  const uint32_t M, const uint32_t N, const uint32_t P, 
                  const uint8_t spN, const uint8_t spM){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P * spN / spM;
    uint32_t vl, vl_dump;

    do{ // outer loop 
        asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        for (uint32_t m = 0; m < M; m+=4){ // mid loop
            uint16_t *p_c  = matrix_c + m*P + p;
            uint16_t *p_c2 = p_c + P;
            uint16_t *p_c3 = p_c + 2*P;
            uint16_t *p_c4 = p_c + 3*P;
            for (uint32_t n = 0; n < N; n++){
                // load a
                uint8_t *p_a  = &matrix_a[m*N + n];
                uint8_t *p_a2 = p_a + N;
                uint8_t *p_a3 = p_a + 2*N;
                uint8_t *p_a4 = p_a + 3*N;
                asm volatile("flw ft0, (%0)" :: "r"(p_a));
                asm volatile("flw ft1, (%0)" :: "r"(p_a2));
                asm volatile("flw ft2, (%0)" :: "r"(p_a3));
                asm volatile("flw ft3, (%0)" :: "r"(p_a4));


                // load b vec
                // TODO: adapt to other formats
                // uint8_t *p_b = &matrix_b[p + n*P*spN/spM];
                
                uint8_t *p_b = &matrix_b[p + n*P/2];
                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));

                // load index 
                // uint8_t *p_index = &index_b[(p + n*P*spN/spM)/IDX_PER_BYTE];
                uint8_t *p_index = &index_b[(p + n*P/2)/IDX_PER_BYTE];
                asm volatile("vle8.v v4, (%0)" ::"r"(p_index));

                if (n==0){ // first iter
                    // reset registers
                    asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl_dump) : "r"(2*avl));
                    asm volatile("vmv.v.i v8, 0");
                    asm volatile("vmv.v.i v12, 0");
                    asm volatile("vmv.v.i v16, 0");
                    asm volatile("vmv.v.i v20, 0");
                    asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
                    // vfwxmul.vf _inp, _idx, _fp, _oup
                    // v8 <-- ft0, v4, v0
                    asm volatile(
                        ".word  (0b110011  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b00100   << 15) | \
                                (0b000     << 12) | \
                                (0b01000   <<  7) | \
                                (0b1010110 <<  0)   \n");
                    // v12 <-- ft1, v4, v0
                    asm volatile(
                        ".word  (0b110011  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b00100   << 15) | \
                                (0b001     << 12) | \
                                (0b01100   <<  7) | \
                                (0b1010110 <<  0)   \n");
                    // v16 <-- ft2, v4, v0
                    asm volatile(
                        ".word  (0b110011  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b00100   << 15) | \
                                (0b010     << 12) | \
                                (0b10000   <<  7) | \
                                (0b1010110 <<  0)   \n");
                    // v20 <-- ft3, v4, v0
                    asm volatile(
                        ".word  (0b110011  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b00100   << 15) | \
                                (0b011     << 12) | \
                                (0b10100   <<  7) | \
                                (0b1010110 <<  0)   \n");
                    
                } else {
                    // vfwxmacc.vf _inp, _idx, _fp, _oup
                    asm volatile(
                        ".word  (0b110001  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b00100   << 15) | \
                                (0b000     << 12) | \
                                (0b01000   <<  7) | \
                                (0b1010110 <<  0)   \n");
                    asm volatile(
                        ".word  (0b110001  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b00100   << 15) | \
                                (0b001     << 12) | \
                                (0b01100   <<  7) | \
                                (0b1010110 <<  0)   \n");
                    asm volatile(
                        ".word  (0b110001  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b00100   << 15) | \
                                (0b010     << 12) | \
                                (0b10000   <<  7) | \
                                (0b1010110 <<  0)   \n");
                    asm volatile(
                        ".word  (0b110001  << 26) | \
                                (0b1       << 25) | \
                                (0b00000   << 20) | \
                                (0b00100   << 15) | \
                                (0b011     << 12) | \
                                (0b10100   <<  7) | \
                                (0b1010110 <<  0)   \n");
                }
                // flex_timer_end();
            }
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl_dump) : "r"(avl*2));
            asm volatile("vse16.v v8, (%0)" ::"r"(p_c));
            asm volatile("vse16.v v12, (%0)" ::"r"(p_c2));
            asm volatile("vse16.v v16, (%0)" ::"r"(p_c3));
            asm volatile("vse16.v v20, (%0)" ::"r"(p_c4));
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        }
        avl -= vl;
        p += vl;
    } while (avl>0);
}

void spatz_AspB_matmul_unroll2x2_wxfp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, uint8_t* index_b,
                  const uint32_t M, const uint32_t N, const uint32_t P, 
                  const uint8_t spN, const uint8_t spM){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P * spN / spM;
    uint32_t vl, vl_dump;

    do{ // outer loop 
        asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        for (uint32_t m = 0; m < M; m+=2){ // mid loop
            uint16_t *p_c  = matrix_c + m*P + p;
            uint16_t *p_c2 = p_c + P;

            // load A: first column
            uint8_t *p_a00  = &matrix_a[m*N];
            uint8_t *p_a10 = p_a00 + N;
            asm volatile("flw ft0, (%0)" :: "r"(p_a00));
            asm volatile("flw ft1, (%0)" :: "r"(p_a10));
            // load A: preload second column
            uint8_t *p_a01  = &matrix_a[m*N + 1];
            uint8_t *p_a11 = p_a01 + N;
            asm volatile("flw ft2, (%0)" :: "r"(p_a01));
            asm volatile("flw ft3, (%0)" :: "r"(p_a11));

            // load B: first row
            uint8_t *p_b0 = &matrix_b[p];
            asm volatile("vle8.v v0, (%0)" ::"r"(p_b0));
            // load Index: first row
            uint8_t *p_index0 = &index_b[p/IDX_PER_BYTE];
            asm volatile("vle8.v v4, (%0)" ::"r"(p_index0));

            // load B: preload second row
            uint8_t *p_b1 = &matrix_b[p + P/2];
            asm volatile("vle8.v v2, (%0)" ::"r"(p_b1));
            // load index: preload second row
            uint8_t *p_index1 = &index_b[(p + P/2)/IDX_PER_BYTE];
            asm volatile("vle8.v v6, (%0)" ::"r"(p_index1));

            // reset registers
            asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl_dump) : "r"(2*avl));
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v12, 0");
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));

            for (uint32_t n = 0; n < N; n+=2){
                // first iteration: vfwxmacc.vf _inp, _idx, _fp, _oup
                // v8 <-- ft0, v4, v0
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00000   << 20) | \
                            (0b00100   << 15) | \
                            (0b000     << 12) | \
                            (0b01000   <<  7) | \
                            (0b1010110 <<  0)   \n");
                // v12 <-- ft1, v4, v0
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00000   << 20) | \
                            (0b00100   << 15) | \
                            (0b001     << 12) | \
                            (0b01100   <<  7) | \
                            (0b1010110 <<  0)   \n");

                // preload A: first set
                p_a00  = &matrix_a[m*N + n + 2];
                p_a10 = p_a00 + N;
                asm volatile("flw ft0, (%0)" :: "r"(p_a00));
                asm volatile("flw ft1, (%0)" :: "r"(p_a10));
                // preload B: first set
                p_b0 = &matrix_b[p + (n+2)*P/2];
                asm volatile("vle8.v v0, (%0)" ::"r"(p_b0));
                // preload index: first set
                p_index0 = &index_b[(p + (n+2)*P/2)/IDX_PER_BYTE];
                asm volatile("vle8.v v4, (%0)" ::"r"(p_index0));

                // second iteration: vfwxmacc.vf _inp, _idx, _fp, _oup
                // v8 <-- ft2, v6, v2
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00010   << 20) | \
                            (0b00110   << 15) | \
                            (0b010     << 12) | \
                            (0b01000   <<  7) | \
                            (0b1010110 <<  0)   \n");
                // v12 <-- ft3, v6, v2
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00010   << 20) | \
                            (0b00110   << 15) | \
                            (0b011     << 12) | \
                            (0b01100   <<  7) | \
                            (0b1010110 <<  0)   \n");

                // preload A: second set
                p_a01  = &matrix_a[m*N + n + 3];
                p_a11 = p_a01 + N;
                asm volatile("flw ft2, (%0)" :: "r"(p_a01));
                asm volatile("flw ft3, (%0)" :: "r"(p_a11));
                // preload B: second set
                p_b1 = &matrix_b[p + (n+3)*P/2];
                asm volatile("vle8.v v2, (%0)" ::"r"(p_b1));
                // preload index: second set
                p_index1 = &index_b[(p + (n+3)*P/2)/IDX_PER_BYTE];
                asm volatile("vle8.v v6, (%0)" ::"r"(p_index1));
            }
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl_dump) : "r"(avl*2));
            asm volatile("vse16.v v8, (%0)" ::"r"(p_c));
            asm volatile("vse16.v v12, (%0)" ::"r"(p_c2));
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        }
        avl -= vl;
        p += vl;
    } while (avl>0);
}

void spatz_AspB_matmul_unroll4x2_wxfp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, uint8_t* index_b,
                  const uint32_t M, const uint32_t N, const uint32_t P, 
                  const uint8_t spN, const uint8_t spM){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P * spN / spM;
    uint32_t vl, vl_dump;

    do{ // outer loop 
        asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        for (uint32_t m = 0; m < M; m+=4){ // mid loop
            uint16_t *p_c  = matrix_c + m*P + p;
            uint16_t *p_c2 = p_c + P;
            uint16_t *p_c3 = p_c + 2*P;
            uint16_t *p_c4 = p_c + 3*P;

            // load A: first column
            uint8_t *p_a00  = &matrix_a[m*N];
            uint8_t *p_a10 = p_a00 + N;
            uint8_t *p_a20 = p_a00 + 2*N;
            uint8_t *p_a30 = p_a00 + 3*N;
            asm volatile("flw ft0, (%0)" :: "r"(p_a00));
            asm volatile("flw ft1, (%0)" :: "r"(p_a10));
            asm volatile("flw ft4, (%0)" :: "r"(p_a20));
            asm volatile("flw ft5, (%0)" :: "r"(p_a30));
            // load A: preload second column
            uint8_t *p_a01  = &matrix_a[m*N + 1];
            uint8_t *p_a11 = p_a01 + N;
            uint8_t *p_a21 = p_a01 + 2*N;
            uint8_t *p_a31 = p_a01 + 3*N;
            asm volatile("flw ft2, (%0)" :: "r"(p_a01));
            asm volatile("flw ft3, (%0)" :: "r"(p_a11));
            asm volatile("flw ft6, (%0)" :: "r"(p_a21));
            asm volatile("flw ft7, (%0)" :: "r"(p_a31));

            // load B: first row
            uint8_t *p_b0 = &matrix_b[p];
            asm volatile("vle8.v v0, (%0)" ::"r"(p_b0));
            // load Index: first row
            uint8_t *p_index0 = &index_b[p/IDX_PER_BYTE];
            asm volatile("vle8.v v4, (%0)" ::"r"(p_index0));

            // load B: preload second row
            uint8_t *p_b1 = &matrix_b[p + P/2];
            asm volatile("vle8.v v2, (%0)" ::"r"(p_b1));
            // load index: preload second row
            uint8_t *p_index1 = &index_b[(p + P/2)/IDX_PER_BYTE];
            asm volatile("vle8.v v6, (%0)" ::"r"(p_index1));

            // reset registers
            asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl_dump) : "r"(2*avl));
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v12, 0");
            asm volatile("vmv.v.i v16, 0");
            asm volatile("vmv.v.i v20, 0");
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));

            for (uint32_t n = 0; n < N; n+=2){
                // first iteration: vfwxmacc.vf _inp, _idx, _fp, _oup
                // v8 <-- ft0, v4, v0
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00000   << 20) | \
                            (0b00100   << 15) | \
                            (0b000     << 12) | \
                            (0b01000   <<  7) | \
                            (0b1010110 <<  0)   \n");
                // v12 <-- ft1, v4, v0
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00000   << 20) | \
                            (0b00100   << 15) | \
                            (0b001     << 12) | \
                            (0b01100   <<  7) | \
                            (0b1010110 <<  0)   \n");
                // v16 <-- ft4, v4, v0
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00000   << 20) | \
                            (0b00100   << 15) | \
                            (0b100     << 12) | \
                            (0b10000   <<  7) | \
                            (0b1010110 <<  0)   \n");
                // v20 <-- ft5, v4, v0
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00000   << 20) | \
                            (0b00100   << 15) | \
                            (0b101     << 12) | \
                            (0b10100   <<  7) | \
                            (0b1010110 <<  0)   \n");

                // preload A: first set
                p_a00  = &matrix_a[m*N + n + 2];
                p_a10 = p_a00 + N;
                p_a20 = p_a00 + 2*N;
                p_a30 = p_a00 + 3*N;
                asm volatile("flw ft0, (%0)" :: "r"(p_a00));
                asm volatile("flw ft1, (%0)" :: "r"(p_a10));
                asm volatile("flw ft4, (%0)" :: "r"(p_a20));
                asm volatile("flw ft5, (%0)" :: "r"(p_a30));
                // preload B: first set
                p_b0 = &matrix_b[p + (n+2)*P/2];
                asm volatile("vle8.v v0, (%0)" ::"r"(p_b0));
                // preload index: first set
                p_index0 = &index_b[(p + (n+2)*P/2)/IDX_PER_BYTE];
                asm volatile("vle8.v v4, (%0)" ::"r"(p_index0));

                // second iteration: vfwxmacc.vf _inp, _idx, _fp, _oup
                // v8 <-- ft2, v6, v2
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00010   << 20) | \
                            (0b00110   << 15) | \
                            (0b010     << 12) | \
                            (0b01000   <<  7) | \
                            (0b1010110 <<  0)   \n");
                // v12 <-- ft3, v6, v2
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00010   << 20) | \
                            (0b00110   << 15) | \
                            (0b011     << 12) | \
                            (0b01100   <<  7) | \
                            (0b1010110 <<  0)   \n");
                // v16 <-- ft6, v6, v2
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00010   << 20) | \
                            (0b00110   << 15) | \
                            (0b110     << 12) | \
                            (0b10000   <<  7) | \
                            (0b1010110 <<  0)   \n");
                // v20 <-- ft7, v6, v2
                asm volatile(
                    ".word  (0b110001  << 26) | \
                            (0b1       << 25) | \
                            (0b00010   << 20) | \
                            (0b00110   << 15) | \
                            (0b111     << 12) | \
                            (0b10100   <<  7) | \
                            (0b1010110 <<  0)   \n");

                // preload A: second set
                p_a01  = &matrix_a[m*N + n + 3];
                p_a11 = p_a01 + N;
                p_a21 = p_a01 + 2*N;
                p_a31 = p_a01 + 3*N;
                asm volatile("flw ft2, (%0)" :: "r"(p_a01));
                asm volatile("flw ft3, (%0)" :: "r"(p_a11));
                asm volatile("flw ft6, (%0)" :: "r"(p_a21));
                asm volatile("flw ft7, (%0)" :: "r"(p_a31));
                // preload B: second set
                p_b1 = &matrix_b[p + (n+3)*P/2];
                asm volatile("vle8.v v2, (%0)" ::"r"(p_b1));
                // preload index: second set
                p_index1 = &index_b[(p + (n+3)*P/2)/IDX_PER_BYTE];
                asm volatile("vle8.v v6, (%0)" ::"r"(p_index1));
            }
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl_dump) : "r"(avl*2));
            asm volatile("vse16.v v8, (%0)" ::"r"(p_c));
            asm volatile("vse16.v v12, (%0)" ::"r"(p_c2));
            asm volatile("vse16.v v16, (%0)" ::"r"(p_c3));
            asm volatile("vse16.v v20, (%0)" ::"r"(p_c4));
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        }
        avl -= vl;
        p += vl;
    } while (avl>0);
}

#endif