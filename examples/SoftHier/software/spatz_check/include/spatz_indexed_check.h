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

// RVV indexed load and store instruction tests

#ifndef _INDEXED_H_
#define _INDEXED_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_libfp16.h"

#define _AVL (128)
#define PRECISION_8BIT  (8)
#define PRECISION_16BIT (16)
#define PRECISION_32BIT (32)
#define PRECISION_64BIT (64)

#pragma GCC optimize("no-tree-loop-distribute-patterns")

// Indexed load
uint8_t test_ele8_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Load Tests: 8-bit ele, 8-bit index] ");
    uint8_t * inp_8 = (uint8_t *)ref_inp;
    uint8_t * ind_8 = (uint8_t *)ref_ind;
    uint8_t * oup_8 = (uint8_t *)ref_oup;
    uint8_t * exp_8 = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp_8[i] = i+1;
    // init index
    for (int i = 0; i < _AVL; ++i) ind_8[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp_8[i] = 2 * inp_8[ind_8[i]];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle8.v v0, (%0)" ::"r"(ind_8));
        asm volatile("vluxei8.v v8, (%0), v0" ::"r"(inp_8));
        asm volatile("vadd.vv v16, v8, v8");
        asm volatile("vse8.v v16,  (%0)" ::"r"(oup_8));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < _AVL; ++i){
        if (exp_8[i] != oup_8[i]) err+=1;
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);
    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp_8[i*8+j] = 0;
        }
        ind_8[i] = 0;
        exp_8[i] = 0;
        oup_8[i] = 0;
    }
    return err;
}

uint8_t test_ele8_index16(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Load Tests: 8-bit ele, 16-bit index] ");
    uint8_t  * inp = (uint8_t *)ref_inp;
    uint16_t * ind = (uint16_t *)ref_ind;
    uint8_t  * oup = (uint8_t *)ref_oup;
    uint8_t  * exp = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[i] = 2 * inp[ind[i]];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle16.v v0, (%0)" ::"r"(ind));
        asm volatile("vluxei16.v v8, (%0), v0" ::"r"(inp));
        asm volatile("vadd.vv v16, v8, v8");
        asm volatile("vse8.v v16,  (%0)" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < _AVL; ++i){
        if (exp[i] != oup[i]) err+=1;
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_ele8_index32(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Load Tests: 8-bit ele, 32-bit index] ");
    uint8_t  * inp = (uint8_t *)ref_inp;
    uint32_t * ind = (uint32_t *)ref_ind;
    uint8_t  * oup = (uint8_t *)ref_oup;
    uint8_t  * exp = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[i] = 2 * inp[ind[i]];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle32.v v0, (%0)" ::"r"(ind));
        asm volatile("vluxei32.v v8, (%0), v0" ::"r"(inp));
        asm volatile("vadd.vv v16, v8, v8");
        asm volatile("vse8.v v16,  (%0)" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < _AVL; ++i){
        if (exp[i] != oup[i]) err+=1;
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_ele8_index64(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Load Tests: 8-bit ele, 64-bit index] ");
    uint8_t  * inp = (uint8_t *)ref_inp;
    uint64_t * ind = (uint64_t *)ref_ind;
    uint8_t  * oup = (uint8_t *)ref_oup;
    uint8_t  * exp = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[i] = 2 * inp[ind[i]];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle64.v v0, (%0)" ::"r"(ind));
        asm volatile("vluxei64.v v8, (%0), v0" ::"r"(inp));
        asm volatile("vadd.vv v16, v8, v8");
        asm volatile("vse8.v v16,  (%0)" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < _AVL; ++i){
        if (exp[i] != oup[i]) err+=1;
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_ele16_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Load Tests: 16-bit ele, 8-bit index] ");
    uint16_t  * inp = (uint16_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint16_t  * oup = (uint16_t *)ref_oup;
    uint16_t  * exp = (uint16_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[i] = 2 * inp[ind[i]];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle8.v v0, (%0)" ::"r"(ind));
        asm volatile("vluxei8.v v8, (%0), v0" ::"r"(inp));
        asm volatile("vadd.vv v16, v8, v8");
        asm volatile("vse16.v v16,  (%0)" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < _AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_ele32_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Load Tests: 32-bit ele, 8-bit index] ");
    uint32_t  * inp = (uint32_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint32_t  * oup = (uint32_t *)ref_oup;
    uint32_t  * exp = (uint32_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[i] = 2 * inp[ind[i]];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle8.v v0, (%0)" ::"r"(ind));
        asm volatile("vluxei8.v v8, (%0), v0" ::"r"(inp));
        asm volatile("vadd.vv v16, v8, v8");
        asm volatile("vse32.v v16,  (%0)" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < _AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_ele64_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Load Tests: 64-bit ele, 8-bit index] ");
    uint64_t  * inp = (uint64_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint64_t  * oup = (uint64_t *)ref_oup;
    uint64_t  * exp = (uint64_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[i] = 2 * inp[ind[i]];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle8.v v0, (%0)" ::"r"(ind));
        asm volatile("vluxei8.v v8, (%0), v0" ::"r"(inp));
        asm volatile("vadd.vv v16, v8, v8");
        asm volatile("vse64.v v16,  (%0)" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < _AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

// Indexed store
uint8_t test_st_ele8_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Store Tests: 8-bit ele, 8-bit index] ");
    uint8_t  * inp = (uint8_t *)ref_inp;
    uint8_t  * ind = (uint8_t *)ref_ind;
    uint8_t  * oup = (uint8_t *)ref_oup;
    uint8_t  * exp = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL; ++i) inp[i] = i+1;
    // init output
    for (int i = 0; i < 8 * _AVL; ++i) {
        oup[i] = 0;
        exp[i] = 0;
    }
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[ind[i]] = inp[i];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e8, m2, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle8.v v0, (%0)" ::"r"(ind));
        asm volatile("vle8.v v2, (%0)" ::"r"(inp));
        asm volatile("vsuxei8.v v2, (%0), v0" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < 8*_AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_st_ele16_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Store Tests: 16-bit ele, 8-bit index] ");
    uint16_t  * inp = (uint16_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint16_t  * oup = (uint16_t *)ref_oup;
    uint16_t  * exp = (uint16_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL; ++i) inp[i] = i+1;
    // init output
    for (int i = 0; i < 8 * _AVL; ++i) {
        oup[i] = 0;
        exp[i] = 0;
    }
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[ind[i]] = inp[i];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle8.v  v0, (%0)" ::"r"(ind));
        asm volatile("vle16.v v2, (%0)" ::"r"(inp));
        asm volatile("vsuxei8.v v2, (%0), v0" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < 8*_AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_st_ele32_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Store Tests: 32-bit ele, 8-bit index] ");
    uint32_t  * inp = (uint32_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint32_t  * oup = (uint32_t *)ref_oup;
    uint32_t  * exp = (uint32_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL; ++i) inp[i] = i+1;
    // init output
    for (int i = 0; i < 8 * _AVL; ++i) {
        oup[i] = 0;
        exp[i] = 0;
    }
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[ind[i]] = inp[i];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle8.v  v0, (%0)" ::"r"(ind));
        asm volatile("vle32.v v2, (%0)" ::"r"(inp));
        asm volatile("vsuxei8.v v2, (%0), v0" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < 8*_AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_st_ele64_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Store Tests: 64-bit ele, 8-bit index] ");
    uint64_t  * inp = (uint64_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint64_t  * oup = (uint64_t *)ref_oup;
    uint64_t  * exp = (uint64_t *)local(0x10000);

    // init vector
    for (int i = 0; i < _AVL; ++i) inp[i] = i+1;
    // init output
    for (int i = 0; i < 8 * _AVL; ++i) {
        oup[i] = 0;
        exp[i] = 0;
    }
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[ind[i]] = inp[i];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e64, m2, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle8.v  v0, (%0)" ::"r"(ind));
        asm volatile("vle64.v v2, (%0)" ::"r"(inp));
        asm volatile("vsuxei8.v v2, (%0), v0" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    int8_t err = 0;
    for (int i = 0; i < 8*_AVL; ++i){
        if (exp[i] != oup[i]){
            err = err + 1;
            printf("exp: %d, oup: %d, err: %d\n", exp[i], oup[i], err);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_st_ele16_index16(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Store Tests: 16-bit ele, 16-bit index] ");
    uint16_t  * inp = (uint16_t *)ref_inp;
    uint16_t  * ind = (uint16_t *)ref_ind;
    uint16_t  * oup = (uint16_t *)ref_oup;
    uint16_t  * exp = (uint16_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL; ++i) inp[i] = i+1;
    // init output
    for (int i = 0; i < 8 * _AVL; ++i) {
        oup[i] = 0;
        exp[i] = 0;
    }
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[ind[i]] = inp[i];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle16.v  v0, (%0)" ::"r"(ind));
        asm volatile("vle16.v v2, (%0)" ::"r"(inp));
        asm volatile("vsuxei16.v v2, (%0), v0" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < 8*_AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_st_ele16_index32(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Store Tests: 16-bit ele, 32-bit index] ");
    uint16_t  * inp = (uint16_t *)ref_inp;
    uint32_t  * ind = (uint32_t *)ref_ind;
    uint16_t  * oup = (uint16_t *)ref_oup;
    uint16_t  * exp = (uint16_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL; ++i) inp[i] = i+1;
    // init output
    for (int i = 0; i < 8 * _AVL; ++i) {
        oup[i] = 0;
        exp[i] = 0;
    }
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[ind[i]] = inp[i];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle32.v  v0, (%0)" ::"r"(ind));
        asm volatile("vle16.v v2, (%0)" ::"r"(inp));
        asm volatile("vsuxei32.v v2, (%0), v0" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < 8*_AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

uint8_t test_st_ele16_index64(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Store Tests: 16-bit ele, 64-bit index] ");
    uint16_t  * inp = (uint16_t *)ref_inp;
    uint64_t  * ind = (uint64_t *)ref_ind;
    uint16_t  * oup = (uint16_t *)ref_oup;
    uint16_t  * exp = (uint16_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL; ++i) inp[i] = i+1;
    // init output
    for (int i = 0; i < 8 * _AVL; ++i) {
        oup[i] = 0;
        exp[i] = 0;
    }
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    // calculate expected output
    for (int i = 0; i < _AVL; ++i){
        exp[ind[i]] = inp[i];
    }

    // index load
    avl = _AVL;
    do{
        asm volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(avl));
        asm volatile("vle64.v  v0, (%0)" ::"r"(ind));
        asm volatile("vle16.v v2, (%0)" ::"r"(inp));
        asm volatile("vsuxei64.v v2, (%0), v0" ::"r"(oup));
        avl -= vl;
    } while (avl>0);

    uint8_t err = 0;
    for (int i = 0; i < 8*_AVL; ++i){
        if (exp[i] != oup[i]){
            err+=1;
            printf("exp: %d, oup: %d\n", exp[i], oup[i]);
        } 
    }
    printf(">>> %d error(s) out of %d checks\n\n", err, _AVL);

    // clean up
    for (int i = 0; i < _AVL; ++i){
        for (int j=0; j<8; j++){
            inp[i*8+j] = 0;
        }
        ind[i] = 0;
        exp[i] = 0;
        oup[i] = 0;
    }
    return err;
}

void test_spatz_indexed(){
    flex_global_barrier_xy();//Global barrier
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
    {
        //////////////////////
        /* Index Load Tests */
        //////////////////////
        uint8_t tot_err = 0;
        void * ref_inp = (void *)local(0x2000);
        void * ref_ind = (void *)local(0x4000);
        void * ref_oup = (void *)local(0x6000);
        tot_err += test_ele8_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index16(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index32(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index64(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele16_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele32_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele64_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele8_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele16_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele32_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele64_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele16_index16(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele16_index32(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele16_index64(ref_inp, ref_ind, ref_oup);

        ref_inp = (void *)local(0x2003);
        tot_err += test_ele8_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index16(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index32(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index64(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele8_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele16_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele32_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele64_index8(ref_inp, ref_ind, ref_oup);

        ref_inp = (void *)local(0x2000);
        ref_ind = (void *)local(0x4005);
        tot_err += test_ele8_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele16_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele32_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele64_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele8_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele16_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele32_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_st_ele64_index8(ref_inp, ref_ind, ref_oup);

        printf(">>> tot_error: %d\n", tot_err);
    }
    flex_global_barrier_xy();//Global barrier
}

#endif