#ifndef _HELLO_WORLD_H_
#define _HELLO_WORLD_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_libfp16.h"

#define _AVL (32)
#pragma GCC optimize("no-tree-loop-distribute-patterns")

void test_spatz(){
    flex_global_barrier_xy();//Global barrier
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
    {
        uint32_t vl;
        uint16_t * x = (uint16_t *)local(0x1000);
        uint16_t * y = (uint16_t *)local(0x2000);

        //initialize vector
        for (int i = 0; i < 8; ++i)
        {
            x[i] = 0x3800; //0.5 
            y[i] = 0x0000; //0.0
        }

        //log out initialization
        printf("[Initialize Vector X]\n");
        for (int i = 0; i < 8; ++i)
        {
            printf("  %f\n", fp16_to_float(x[i]));
        }
        printf("[Initialize Vector Y]\n");
        for (int i = 0; i < 8; ++i)
        {
            printf("  %f\n", fp16_to_float(y[i]));
        }
        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(8));
        asm volatile("vle16.v v0,  (%0)" ::"r"(x));
        asm volatile("vfadd.vv v8, v0, v0");
        asm volatile("vse16.v v8,  (%0)" ::"r"(y));
        printf("[Vector Operation: Y = X + X]\n");
        for (int i = 0; i < 8; ++i)
        {
            printf("  %f\n", fp16_to_float(y[i]));
        }
        asm volatile("vfmul.vv v8, v0, v0");
        asm volatile("vse16.v v8,  (%0)" ::"r"(y));
        printf("[Vector Operation: Y = X * X]\n");
        for (int i = 0; i < 8; ++i)
        {
            printf("  %f\n", fp16_to_float(y[i]));
        }
        asm_rvv_exp(0,8);
        asm volatile("vse16.v v8,  (%0)" ::"r"(y));
        printf("[Vector Operation: Y = exp(X)]\n");
        for (int i = 0; i < 8; ++i)
        {
            printf("  %f\n", fp16_to_float(y[i]));
        }

        // bowwang: vid.v instr check
        uint8_t * z    = (uint8_t *)local(0x2000);

        for (int i = 0; i < 8; ++i) z[i] = 0;

        printf("[vid.v SEW=8 check: Initialize Vector Z]\n");
        for (int i = 0; i < 8; ++i) printf("  %d\n", z[i]);

        asm volatile("vsetvli %0, %1, e8, m8, ta, ma" : "=r"(vl) : "r"(8));
        asm volatile(
            "vid.v v0\n"
            "vse8.v v0, (%0)\n"
            :
            : "r"(z)
            : "v0"
        );

        printf("[Updated Vector Z]\n");
        for (int i = 0; i < 8; ++i) printf("  %d\n", z[i]);

        // SEW=16
        uint16_t * z_16    = (uint16_t *)local(0x2000);

        for (int i = 0; i < 8; ++i) z_16[i] = 0;

        printf("[vid.v SEW=16 check: Initialize Vector Z]\n");
        for (int i = 0; i < 8; ++i) printf("  %d\n", z_16[i]);

        asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(vl) : "r"(8));
        asm volatile(
            "vid.v v0\n"
            "vse16.v v0, (%0)\n"
            :
            : "r"(z_16)
            : "v0"
        );

        printf("[Updated Vector Z]\n");
        for (int i = 0; i < 8; ++i) printf("  %d\n", z_16[i]);

        // bowwang: configurable VLSU buswidth check
        uint32_t avl = _AVL;
        uint16_t * x_8 = (uint16_t *)local(0x2002);
        uint16_t * y_8 = (uint16_t *)local(0x8004);

        for (int i = 0; i < avl; ++i)
        {
            if      (i%3==0) x_8[i] = 1; 
            else if (i%3==1) x_8[i] = 2; 
            else             x_8[i] = 3;
            y_8[i] = 0;
        }

        printf("[Initialize Vector X]\n");
        for (int i = 0; i < avl; i+=8) {
            printf("%-5d-%-5d >  ", i, i+7);
            for (int j=0; j<8; j++) printf("%-10d", x_8[i+j]);
            printf("\n");
        }

        do{
            asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle16.v v0,  (%0)" ::"r"(x_8));
            asm volatile("vadd.vv v8, v0, v0");
            asm volatile("vse16.v v8,  (%0)" ::"r"(y_8));
            avl -= vl;
        } while (avl>0);

        // restore avl
        avl = _AVL;
        printf("[Vector Operation: Y = X + X]\n");
        for (int i = 0; i < avl; i+=8) {
            printf("%-5d-%-5d >  ", i, i+7);
            for (int j=0; j<8; j++) printf("%-10d", y_8[i+j]);
            printf("\n");
        }
    }
    flex_global_barrier_xy();//Global barrier
}

#endif