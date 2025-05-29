#ifndef _INDEXED_H_
#define _INDEXED_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_libfp16.h"

#define _AVL (256)
#pragma GCC optimize("no-tree-loop-distribute-patterns")

void l1_layout(uint8_t *addr, uint8_t size){ // size in byte
    // align the output
    uint8_t offset = ((uint32_t)addr) % (8*8);
    uint8_t * addr_aligned = (uint8_t *)((uint32_t)addr - offset);
    // dump info 8 x 8bytes
    int8_t size_aligned = size + offset;

    do{
        printf("0x%8x - 0x%8x >  ", addr_aligned, addr_aligned+8*8-1);
        for (int j=0; j<4; j++){
            for (int k=0; k<8; k++){
                if (k<7) printf("%2x_", addr_aligned[j+k]);
                else     printf("%2x", addr_aligned[j+k]);
            }
            printf("  |  ");
        }
        printf("\n");
        addr_aligned += 4*8;
        size_aligned -= 4*8;
    } while (size_aligned>0);
}

uint8_t test_ele8_index8(void * ref_inp, void * ref_ind, void * ref_oup){
    uint32_t vl;
    uint32_t avl;
    printf("[Index Load Tests: 8-bit ele, 8-bit index]\n");
    uint8_t * inp_8 = (uint8_t *)ref_inp;
    uint8_t * ind_8 = (uint8_t *)ref_ind;
    uint8_t * oup_8 = (uint8_t *)ref_oup;
    uint8_t * exp_8 = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp_8[i] = i+1;
    printf("[Init inp_8, done]\n");
    // init index
    for (int i = 0; i < _AVL; ++i) ind_8[i] = 3*i+1;
    printf("[Init ind_8, done]\n");
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

    printf("[Check indexed load]\n");
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
    printf("[Index Load Tests: 8-bit ele, 16-bit index]\n");
    uint8_t  * inp = (uint8_t *)ref_inp;
    uint16_t * ind = (uint16_t *)ref_ind;
    uint8_t  * oup = (uint8_t *)ref_oup;
    uint8_t  * exp = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    printf("[Init inp, done]\n");
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    printf("[Init ind, done]\n");
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

    printf("[Check indexed load]\n");
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
    printf("[Index Load Tests: 8-bit ele, 32-bit index]\n");
    uint8_t  * inp = (uint8_t *)ref_inp;
    uint32_t * ind = (uint32_t *)ref_ind;
    uint8_t  * oup = (uint8_t *)ref_oup;
    uint8_t  * exp = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    printf("[Init inp, done]\n");
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    printf("[Init ind, done]\n");
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

    printf("[Check indexed load]\n");
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
    printf("[Index Load Tests: 8-bit ele, 64-bit index]\n");
    uint8_t  * inp = (uint8_t *)ref_inp;
    uint64_t * ind = (uint64_t *)ref_ind;
    uint8_t  * oup = (uint8_t *)ref_oup;
    uint8_t  * exp = (uint8_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    printf("[Init inp, done]\n");
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    printf("[Init ind, done]\n");
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

    printf("[Check indexed load]\n");
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
    printf("[Index Load Tests: 16-bit ele, 8-bit index]\n");
    uint16_t  * inp = (uint16_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint16_t  * oup = (uint16_t *)ref_oup;
    uint16_t  * exp = (uint16_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    printf("[Init inp, done]\n");
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    printf("[Init ind, done]\n");
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

    printf("[Check indexed load]\n");
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
    printf("[Index Load Tests: 32-bit ele, 8-bit index]\n");
    uint32_t  * inp = (uint32_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint32_t  * oup = (uint32_t *)ref_oup;
    uint32_t  * exp = (uint32_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    printf("[Init inp, done]\n");
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    printf("[Init ind, done]\n");
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

    printf("[Check indexed load]\n");
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
    printf("[Index Load Tests: 64-bit ele, 8-bit index]\n");
    uint64_t  * inp = (uint64_t *)ref_inp;
    uint8_t   * ind = (uint8_t *)ref_ind;
    uint64_t  * oup = (uint64_t *)ref_oup;
    uint64_t  * exp = (uint64_t *)local(0x8000);

    // init vector
    for (int i = 0; i < _AVL*8; ++i) inp[i] = i+1;
    printf("[Init inp, done]\n");
    // init index
    for (int i = 0; i < _AVL; ++i) ind[i] = 3*i+1;
    printf("[Init ind, done]\n");
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

    printf("[Check indexed load]\n");
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

        ref_inp = (void *)local(0x2003);
        tot_err += test_ele8_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index16(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index32(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele8_index64(ref_inp, ref_ind, ref_oup);

        ref_inp = (void *)local(0x2000);
        ref_ind = (void *)local(0x4005);
        tot_err += test_ele8_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele16_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele32_index8(ref_inp, ref_ind, ref_oup);
        tot_err += test_ele64_index8(ref_inp, ref_ind, ref_oup);

        printf(">>> tot_error: %d\n", tot_err);
    }
    flex_global_barrier_xy();//Global barrier
}

#endif