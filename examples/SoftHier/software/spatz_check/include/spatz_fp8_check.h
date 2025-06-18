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

// RVV instruction check with fp8

#ifndef _SPATZ_FP8_CHECK_H_
#define _SPATZ_FP8_CHECK_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_libfp8.h"

#include "data_spatz_fp8_check.h"

#pragma GCC optimize("no-tree-loop-distribute-patterns")

void test_spatz_fp8(){
    flex_global_barrier_xy();//Global barrier
    flex_alloc_init();
    flex_intra_cluster_sync();
    flex_global_barrier_xy();

    uint32_t CID = flex_get_cluster_id(); // Get cluster ID
    uint32_t core_id = flex_get_core_id();

    if (CID == 0 && core_id == 0)
    {
        uint8_t *vector_a   = (uint8_t *)flex_l1_malloc(_AVL);
        uint8_t *vector_b   = (uint8_t *)flex_l1_malloc(_AVL);
        uint8_t *vector_c   = (uint8_t *)flex_l1_malloc(_AVL);
        uint8_t *vector_res = (uint8_t *)flex_l1_malloc(_AVL);
        // data movement
        for (uint32_t i=0; i<_AVL; i++){
            vector_a[i] = vector_a_fp8[i];
            vector_b[i] = vector_b_fp8[i];
            vector_c[i] = vector_c_fp8[i];
        }

        // load scalar
        asm volatile("flw fa0, (%0)" :: "r"(s_fp8));

        uint32_t avl, vl, p;
        uint8_t *p_a, *p_b, *p_c, *p_res;
        // vfadd.vv
        printf("[ins] vfadd.vv \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_b = vector_b + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v4, (%0)" ::"r"(p_b));
            asm volatile("vfadd.vv v8, v0, v4"); // res = a + b;
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_add_vv_fp8, 0.25f);

        // vfsub.vv
        printf("[ins] vfsub.vv \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_b = vector_b + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v4, (%0)" ::"r"(p_b));
            asm volatile("vfsub.vv v8, v0, v4"); // res = a + b;
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_sub_vv_fp8, 0.25f);

        // vfmin.vv
        printf("[ins] vfmin.vv \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_b = vector_b + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v4, (%0)" ::"r"(p_b));
            asm volatile("vfmin.vv v8, v0, v4"); // res = a + b;
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_min_vv_fp8, 0.25f);

        // vfmax.vv
        printf("[ins] vfmax.vv \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_b = vector_b + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v4, (%0)" ::"r"(p_b));
            asm volatile("vfmax.vv v8, v0, v4"); // res = max(a, b);
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_max_vv_fp8, 0.25f);

        // vfmul.vv
        printf("[ins] vfmul.vv \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_b = vector_b + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v4, (%0)" ::"r"(p_b));
            asm volatile("vfmul.vv v8, v0, v4"); // res = max(a, b);
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_mul_vv_fp8, 0.25f);

        // vfmadd.vv
        printf("[ins] vfmadd.vv \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_b = vector_b + p;
            p_c = vector_c + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v4, (%0)" ::"r"(p_b));
            asm volatile("vle8.v v8, (%0)" ::"r"(p_c));
            asm volatile("vfmadd.vv v8, v0, v4"); // res = a x res(c) + b;
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_madd_vv_fp8, 0.25f);

        // vfmacc.vv
        printf("[ins] vfmacc.vv \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_b = vector_b + p;
            p_c = vector_c + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v4, (%0)" ::"r"(p_b));
            asm volatile("vle8.v v8, (%0)" ::"r"(p_c));
            asm volatile("vfmacc.vv v8, v0, v4"); // res = a x b + res(c);
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_macc_vv_fp8, 0.25f);

        // vfadd.vf
        printf("[ins] vfadd.vf \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vfadd.vf v8, v0, fa0"); // res = a + broadcast(s);
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_add_vf_fp8, 0.25f);

        // vfsub.vf
        printf("[ins] vfsub.vf \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vfsub.vf v8, v0, fa0"); // res = a - broadcast(s);
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_sub_vf_fp8, 0.25f);

        // vfmul.vf
        printf("[ins] vfmul.vf \n");
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vfmul.vf v8, v0, fa0"); // res = a - broadcast(s);
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_mul_vf_fp8, 0.25f);

        // vfmadd.vf
        printf("[ins] vfmadd.vf \n"); // C = C x s + A
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v8, (%0)" ::"r"(p_c));
            asm volatile("vfmadd.vf v8, fa0, v0");
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_madd_vf_fp8, 0.25f);

        // vfmacc.vf
        printf("[ins] vfmacc.vf \n"); // C = A x s + C
        avl = _AVL;
        p = 0;
        do{
            p_a = vector_a + p;
            p_res = vector_res + p;
            asm volatile("vsetvli %0, %1, e8, m4, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle8.v v0, (%0)" ::"r"(p_a));
            asm volatile("vle8.v v8, (%0)" ::"r"(p_c));
            asm volatile("vfmacc.vf v8, fa0, v0");
            asm volatile("vse8.v v8, (%0)" ::"r"(p_res));
            avl -= vl;
            p += vl;
        } while (avl>0);
        spatz_verify(_AVL, vector_res, vector_c_macc_vf_fp8, 0.25f);
    }

    flex_global_barrier_xy();//Global barrier
}

#endif