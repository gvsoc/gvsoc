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

// Customized RVV instruction (vfwxmul.vf, vfwxmacc.vf) check

#ifndef _SPATZ_ISA_CHECK_H_
#define _SPATZ_ISA_CHECK_H_

#include "flex_runtime.h"
#include "flex_printf.h"
// #include "flex_libfp8.h"

#define _AVL (16)

#pragma GCC optimize("no-tree-loop-distribute-patterns")

void test_spatz_isa(){
    flex_global_barrier_xy();//Global barrier
    flex_alloc_init();
    flex_intra_cluster_sync();
    flex_global_barrier_xy();

    uint32_t CID = flex_get_cluster_id(); // Get cluster ID
    uint32_t core_id = flex_get_core_id();

    if (CID == 0 && core_id == 0)
    {

        uint32_t avl, vl, vl_dump;
        uint8_t err = 0;
        avl = _AVL;

        // odd indeices
        __attribute__((section(".hbm"))) static const uint8_t index[_AVL/4] = 
                        {0b11011101, 0b11011101, 0b11011101, 0b11011101};
        // even indices
        __attribute__((section(".hbm"))) static const uint8_t index_macc[_AVL/4] = 
                        {0b10001000, 0b10001000, 0b10001000, 0b10001000};
        // input vector
        __attribute__((section(".hbm"))) static const uint8_t inp_vec[_AVL] = 
                        { 0x36,  0x37,  0xB5,  0x3A,  0x39,  0xAD,  0xB7,  0xB8,
                        0x3B,  0x36,  0xAE,  0xAB,  0x29,  0x29,  0x39,  0x3A,};
        // expected vector
        __attribute__((section(".hbm"))) static const uint16_t exp_vec[_AVL] = 
                    {   0xAF80,  0xB060,  0x2E40,  0xB380,  0xB240,  0x2640,  0x3060,  0x3100,
                        0xB460,  0xAF80,  0x2780,  0x2460,  0xA240,  0xA240,  0xB240,  0xB380,};

        uint8_t * ref_ind = (void *)local(0x2000);
        uint8_t * ref_inp = (void *)local(0x4000);
        uint16_t * oup_vec = (void *)local(0x10000);

        for (int i=0; i<_AVL/4; i++){
            ref_ind[i] = index[i];
        }
        for (int i=0; i<_AVL; i++){
            ref_inp[i] = inp_vec[i];
            oup_vec[i] = 0;
            oup_vec[_AVL+i] = 0;
        }
        uint8_t s_fp8[1] =  {0xB5,};

        // config n:m format (nmconfig.v N:M=2:4)
        asm volatile(
            ".word  (0b100000  << 26) | \
                    (0b1       << 25) | \
                    (0b00000   << 20) | \
                    (0b00100   << 15) | \
                    (0b000     << 12) | \
                    (0b00010   <<  7) | \
                    (0b1010110 <<  0)   \n");

        // vfwxmul.vf
        printf("[ins] vfwxmul.vf \n");
        do{
            asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl_dump) : "r"(2*avl));
            asm volatile("vmv.v.i v8, 0");
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("flw ft0, (%0)" :: "r"(s_fp8));
            asm volatile("vle8.v v0, (%0)" ::"r"(ref_ind));
            asm volatile("vle8.v v4, (%0)" ::"r"(ref_inp));
            asm volatile(
                 ".word (0b110011  << 26) | \
                        (0b1       << 25) | \
                        (0b00100   << 20) | \
                        (0b00000   << 15) | \
                        (0b000     << 12) | \
                        (0b01000   <<  7) | \
                        (0b1010110 <<  0)   \n");
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl_dump) : "r"(avl*2));
            asm volatile("vse16.v v8, (%0)" ::"r"(oup_vec));
            avl -= vl;
        } while (avl>0);
        
        for (int i=0; i<_AVL; i++){
            uint16_t res_odd = oup_vec[2*i + 1];
            uint16_t exp     = exp_vec[i];
            if (res_odd != exp) {
                printf("MISMATCH: exp 0x%x, got 0x%x\n", exp, res_odd);
                err += 1;
            }
        }

        for (int i=0; i<_AVL/4; i++){
            ref_ind[i] = index_macc[i];
        }

        // vfwxmacc.vv
        printf("[ins] vfwxmacc.vf \n");
        avl = _AVL;
        do{
            asm volatile("vsetvli %0, %1, e16, m4, ta, ma" : "=r"(vl_dump) : "r"(2*avl));
            asm volatile("vle16.v v8, (%0)" ::"r"(oup_vec));
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("flw ft0, (%0)" :: "r"(s_fp8));
            asm volatile("vle8.v v0, (%0)" ::"r"(ref_ind));
            asm volatile("vle8.v v4, (%0)" ::"r"(ref_inp));
            asm volatile(
                 ".word (0b110001  << 26) | \
                        (0b1       << 25) | \
                        (0b00100   << 20) | \
                        (0b00000   << 15) | \
                        (0b000     << 12) | \
                        (0b01000   <<  7) | \
                        (0b1010110 <<  0)   \n");
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl_dump) : "r"(avl*2));
            asm volatile("vse16.v v8, (%0)" ::"r"(oup_vec));
            avl -= vl;
        } while (avl>0);
        
        for (int i=0; i<_AVL; i++){
            uint16_t res_odd  = oup_vec[2*i + 1];
            uint16_t res_even = oup_vec[2*i];
            uint16_t exp     = exp_vec[i];
            if (res_odd != exp || res_even != exp) {
                printf("MISMATCH: exp 0x%x, got even 0x%x, got odd 0x%x\n", exp, res_even, res_odd);
                err += 1;
            }
        }

        printf(">>> Total %d error(s) found.\n", err);

    }

    flex_global_barrier_xy();//Global barrier
}

#endif