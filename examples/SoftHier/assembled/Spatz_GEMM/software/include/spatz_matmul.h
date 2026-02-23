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
#include "spatz_rvv_extensions.h"

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

// N:M Configuration helper functions
inline void nm_config(const uint8_t spN, const uint8_t spM){
    if (spM == 2){
        if (spN == 1){ // 1:2 format 
            asm volatile(".word  (0b10000010000000010 << 15) | (0b00000001 << 7) | (0b1010110 << 0)   \n");}
    } else if (spM == 4){
        if (spN == 1){ // 1:4 format
            asm volatile(".word  (0b10000010000000100 << 15) | (0b00000001 << 7) | (0b1010110 << 0)   \n");}
        else if (spN == 2){ // 2:4 format
            asm volatile(".word  (0b10000010000000100 << 15) | (0b00000010 << 7) | (0b1010110 << 0)   \n");}
    } else if (spM == 8){
        if (spN == 1){ // 1:8 format
            asm volatile(".word  (0b10000010000001000 << 15) | (0b00000001 << 7) | (0b1010110 << 0)   \n");} 
        else if (spN == 2){ // 2:8 format
            asm volatile(".word  (0b10000010000001000 << 15) | (0b00000010 << 7) | (0b1010110 << 0)   \n");} 
        else if (spN == 4){ // 4:8 format
            asm volatile(".word  (0b10000010000001000 << 15) | (0b00000100 << 7) | (0b1010110 << 0)   \n");}
    } else if (spM == 16){
        if (spN == 1){ // 1:16 format
            asm volatile(".word  (0b10000010000010000 << 15) | (0b00000001 << 7) | (0b1010110 << 0)   \n");} 
        else if (spN == 2){ // 2:16 format
            asm volatile(".word  (0b10000010000010000 << 15) | (0b00000010 << 7) | (0b1010110 << 0)   \n");} 
        else if (spN == 4){ // 4:16 format
            asm volatile(".word  (0b10000010000010000 << 15) | (0b00000100 << 7) | (0b1010110 << 0)   \n");} 
        else if (spN == 8){ // 8:16 format
            asm volatile(".word  (0b10000010000010000 << 15) | (0b00001000 << 7) | (0b1010110 << 0)   \n");}
    }   
} 

// sparse matmul [Gustav], AspB, input _fp8, index _uint2, output _fp16
// effective output elements are gather/scatter with VLSU
void spatz_AspB_matmul_wxfp16(uint8_t* matrix_a, uint8_t* matrix_b, uint16_t* matrix_c, uint8_t* index_b,
                  const uint32_t M, const uint32_t N, const uint32_t P, 
                  const uint8_t spN, const uint8_t spM){

    // init counters
    uint32_t p = 0;

    uint32_t avl = P * spN / spM;
    uint32_t vl, vl_dump;

    uint8_t index_per_byte=0;
    if      (spM==2)            index_per_byte=8;
    else if (spM==4)            index_per_byte=4;
    else if (spM==8 || spM==16) index_per_byte=2;

    nm_config(spN, spM);

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
                uint8_t *p_b = &matrix_b[p + n*P*spN/spM];
                // uint8_t *p_b = &matrix_b[p + n*P/2];

                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));

                // load index 
                // TODO: adapt to other formats
                uint8_t *p_index = &index_b[(p + n*P*spN/spM)/index_per_byte];
                // uint8_t *p_index = &index_b[(p + n*P/2)/index_per_byte];
                asm volatile("vle8.v v8, (%0)" ::"r"(p_index));

                if (n==0){ // first iter
                    // reset registers
                    asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl_dump) : "r"(spM/spN*avl));
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
            asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl_dump) : "r"(spM/spN*avl));
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

    uint8_t index_per_byte=0;
    if      (spM==2)            index_per_byte=8;
    else if (spM==4)            index_per_byte=4;
    else if (spM==8 || spM==16) index_per_byte=2;

    nm_config(spN, spM);

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
                uint8_t *p_b = &matrix_b[p + n*P*spN/spM];
                // uint8_t *p_b = &matrix_b[p + n*P/2];
                asm volatile("vle8.v v0, (%0)" ::"r"(p_b));

                // load index 
                uint8_t *p_index = &index_b[(p + n*P*spN/spM)/index_per_byte];
                // uint8_t *p_index = &index_b[(p + n*P/2)/index_per_byte];
                asm volatile("vle8.v v4, (%0)" ::"r"(p_index));

                if (n==0){ // first iter
                    // reset registers
                    asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl_dump) : "r"(spM/spN*avl));
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
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl_dump) : "r"(avl*spM/spN));
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

    uint8_t index_per_byte=0;
    if      (spM==2)            index_per_byte=8;
    else if (spM==4)            index_per_byte=4;
    else if (spM==8 || spM==16) index_per_byte=2;

    nm_config(spN, spM);

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
            uint8_t *p_index0 = &index_b[p/index_per_byte];
            asm volatile("vle8.v v4, (%0)" ::"r"(p_index0));

            // load B: preload second row
            uint8_t *p_b1 = &matrix_b[p + P*spN/spM];
            asm volatile("vle8.v v2, (%0)" ::"r"(p_b1));
            // load index: preload second row
            uint8_t *p_index1 = &index_b[(p + P*spN/spM)/index_per_byte];
            asm volatile("vle8.v v6, (%0)" ::"r"(p_index1));

            // reset registers
            asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl_dump) : "r"(spM/spN*avl));
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
                p_b0 = &matrix_b[p + (n+2)*P*spN/spM];
                asm volatile("vle8.v v0, (%0)" ::"r"(p_b0));
                // preload index: first set
                p_index0 = &index_b[(p + (n+2)*P*spN/spM)/index_per_byte];
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
                p_b1 = &matrix_b[p + (n+3)*P*spN/spM];
                asm volatile("vle8.v v2, (%0)" ::"r"(p_b1));
                // preload index: second set
                p_index1 = &index_b[(p + (n+3)*P*spN/spM)/index_per_byte];
                asm volatile("vle8.v v6, (%0)" ::"r"(p_index1));
            }
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl_dump) : "r"(avl*spM/spN));
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

    // params
    uint32_t register spM_over_spN = spM/spN;

    uint32_t avl = P / spM_over_spN;
    uint32_t vl, vl_dump;

    uint8_t index_per_byte=0;
    if      (spM==2)            index_per_byte=8;
    else if (spM==4)            index_per_byte=4;
    else if (spM==8 || spM==16) index_per_byte=2;

    nm_config(spN, spM);

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
            uint8_t *p_index0 = &index_b[p/index_per_byte];
            asm volatile("vle8.v v4, (%0)" ::"r"(p_index0));

            // load B: preload second row
            uint8_t *p_b1 = &matrix_b[p + P/spM_over_spN];
            asm volatile("vle8.v v2, (%0)" ::"r"(p_b1));
            // load index: preload second row
            uint8_t *p_index1 = &index_b[(p + P/spM_over_spN)/index_per_byte];
            asm volatile("vle8.v v6, (%0)" ::"r"(p_index1));

            // reset registers
            asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl_dump) : "r"(spM_over_spN*avl));
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
                p_b0 = &matrix_b[p + (n+2)*P/spM_over_spN];
                asm volatile("vle8.v v0, (%0)" ::"r"(p_b0));
                // preload index: first set
                p_index0 = &index_b[(p + (n+2)*P/spM_over_spN)/index_per_byte];
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
                p_b1 = &matrix_b[p + (n+3)*P/spM_over_spN];
                asm volatile("vle8.v v2, (%0)" ::"r"(p_b1));
                // preload index: second set
                p_index1 = &index_b[(p + (n+3)*P/spM_over_spN)/index_per_byte];
                asm volatile("vle8.v v6, (%0)" ::"r"(p_index1));
            }
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl_dump) : "r"(avl*spM_over_spN));
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

static inline unsigned ipb_to_shift(unsigned ipb) {
    switch (ipb) { case 1: return 0; case 2: return 1; case 4: return 2; case 8: return 3;
                   default: return 0; }
}

void spatz_AspB_matmul_unroll4x2_wxfp16_opt(
    uint8_t* __restrict matrix_a,
    uint8_t* __restrict matrix_b,
    uint16_t* __restrict matrix_c,
    uint8_t* __restrict index_b,
    const uint32_t M, const uint32_t N, const uint32_t P,
    const uint8_t spN, const uint8_t spM)
{
    // params & precomputed shifts
    uint32_t spM_over_spN = spM / spN;              // {2,4,8,16}
    uint32_t rowStride    = P / spM_over_spN;       // == p_over_spM_over_spN
    uint32_t avl          = P / spM_over_spN;       // vector length in bytes (per your code)
    uint32_t vl, vl_dump;

    // index-per-byte (powers of two → shift)
    uint8_t index_per_byte = (spM==2) ? 8 : (spM==4) ? 4 : 2;
    unsigned s_index = ipb_to_shift(index_per_byte);

    // shift for rowStride (since spM_over_spN ∈ {2,4,8,16})
    static const unsigned sh_tab[17] = { [2]=1, [4]=2, [8]=3, [16]=4 };
    unsigned s = sh_tab[spM_over_spN];

    nm_config(spN, spM);

    // handy multiples of rowStride without MUL in the loop
    const uint32_t rs  = rowStride;
    const uint32_t rs2 = rowStride << 1;            // 2*rowStride

    uint32_t p = 0;                                  // running column offset

    do { // outer loop over avl
        asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));

        for (uint32_t m = 0; m < M; m += 4) { // block of 4 rows
            // ---- Pointers for C (carry them, avoid recomputing m*P repeatedly)
            uint16_t *pc  = matrix_c + (size_t)m * P + p;

            // ---- Preload A for the first two columns (n and n+1 at start)
            uint8_t *a_row0 = matrix_a + (size_t)m * N;   // row 0 base
            uint8_t *a_row1 = a_row0 + N;                 // row 1 base
            uint8_t *a_row2 = a_row1 + N;                 // row 2 base
            uint8_t *a_row3 = a_row2 + N;                 // row 3 base
            // column 0
            asm volatile("flw ft0, (%0)" :: "r"(a_row0));
            asm volatile("flw ft2, (%0)" :: "r"(a_row0 + 1));
            // column 0 + N
            asm volatile("flw ft1, (%0)" :: "r"(a_row1));
            asm volatile("flw ft3, (%0)" :: "r"(a_row1 + 1));
            // column 0 + 2N
            asm volatile("flw ft4, (%0)" :: "r"(a_row2));
            asm volatile("flw ft6, (%0)" :: "r"(a_row2 + 1));
            // column 0 + 3N
            asm volatile("flw ft5, (%0)" :: "r"(a_row3));
            asm volatile("flw ft7, (%0)" :: "r"(a_row3 + 1));

            // ---- B and index base for current p
            uint8_t *b_base   = matrix_b + p;
            size_t   off_base = (size_t)p;               // for index shift

            // load B/index for current rows (k = 0 and 1)
            asm volatile("vle8.v v0, (%0)" ::"r"(b_base));             // k=0
            asm volatile("vle8.v v4, (%0)" ::"r"(index_b + (off_base >> s_index)));
            asm volatile("vle8.v v2, (%0)" ::"r"(b_base + rs));        // k=1
            asm volatile("vle8.v v6, (%0)" ::"r"(index_b + ((off_base + rs) >> s_index)));

            // ---- Reset accumulators (once per m-block)
            asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl_dump) : "r"(spM_over_spN * avl));
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vmv.v.i v12, 0");
            asm volatile("vmv.v.i v16, 0");
            asm volatile("vmv.v.i v20, 0");
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));

            // ---- Inner loop: advance in pairs (n, n+1) each iteration
            // Keep rolling bases so we never multiply or divide inside.
            uint8_t *b_cur   = b_base;        // corresponds to n
            size_t   off_cur = off_base;      // for index (n)
            for (uint32_t n = 0; n < N; n += 2) {
                // 1st iter uses: (v0,v4) with ft0/1/4/5
                EMIT_WORD_IMM( VFWXMACC_VF(RVV_V8,  RVV_V0, RVV_V4, RVF_RS1_FT0, 1) );
                EMIT_WORD_IMM( VFWXMACC_VF(RVV_V12, RVV_V0, RVV_V4, RVF_RS1_FT1, 1) );
                EMIT_WORD_IMM( VFWXMACC_VF(RVV_V16, RVV_V0, RVV_V4, RVF_RS1_FT4, 1) );
                EMIT_WORD_IMM( VFWXMACC_VF(RVV_V20, RVV_V0, RVV_V4, RVF_RS1_FT5, 1) );

                // Preload A for (n+2)
                uint32_t col2 = n + 2;
                asm volatile("flw ft0, (%0)" :: "r"(a_row0 + col2));
                asm volatile("flw ft1, (%0)" :: "r"(a_row1 + col2));
                asm volatile("flw ft4, (%0)" :: "r"(a_row2 + col2));
                asm volatile("flw ft5, (%0)" :: "r"(a_row3 + col2));

                // Preload B/index for (n+2) into (v0,v4)
                asm volatile("vle8.v v0, (%0)" ::"r"(b_cur + rs2));
                asm volatile("vle8.v v4, (%0)" ::"r"(index_b + ((off_cur + rs2) >> s_index)));

                // 2nd iter uses: (v2,v6) with ft2/3/6/7
                EMIT_WORD_IMM( VFWXMACC_VF(RVV_V8,  RVV_V2, RVV_V6, RVF_RS1_FT2, 1) );
                EMIT_WORD_IMM( VFWXMACC_VF(RVV_V12, RVV_V2, RVV_V6, RVF_RS1_FT3, 1) );
                EMIT_WORD_IMM( VFWXMACC_VF(RVV_V16, RVV_V2, RVV_V6, RVF_RS1_FT6, 1) );
                EMIT_WORD_IMM( VFWXMACC_VF(RVV_V20, RVV_V2, RVV_V6, RVF_RS1_FT7, 1) );

                // Preload A for (n+3)
                uint32_t col3 = col2 + 1;
                asm volatile("flw ft2, (%0)" :: "r"(a_row0 + col3));
                asm volatile("flw ft3, (%0)" :: "r"(a_row1 + col3));
                asm volatile("flw ft6, (%0)" :: "r"(a_row2 + col3));
                asm volatile("flw ft7, (%0)" :: "r"(a_row3 + col3));

                // Preload B/index for (n+3) into (v2,v6)
                asm volatile("vle8.v v2, (%0)" ::"r"(b_cur + rs2 + rs)); // rs*3 without MUL
                asm volatile("vle8.v v6, (%0)" ::"r"(index_b + ((off_cur + rs2 + rs) >> s_index)));

                // advance bases for next (n+=2)
                b_cur   += rs2;
                off_cur += rs2;
            }

            // ---- Store C rows
            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl_dump) : "r"(avl * spM_over_spN));
            asm volatile("vse16.v v8,  (%0)" ::"r"(pc));
            pc += P;
            asm volatile("vse16.v v12, (%0)" ::"r"(pc));
            pc += P;
            asm volatile("vse16.v v16, (%0)" ::"r"(pc));
            pc += P;
            asm volatile("vse16.v v20, (%0)" ::"r"(pc));

            asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        }

        avl -= vl;
        p   += vl;
    } while (avl > 0);
}


#endif