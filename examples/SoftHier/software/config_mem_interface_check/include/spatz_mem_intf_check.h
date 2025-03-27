#ifndef _HELLO_WORLD_H_
#define _HELLO_WORLD_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_libfp16.h"

#define _AVL (64)
#pragma GCC optimize("no-tree-loop-distribute-patterns")

void test_spatz(){
    flex_global_barrier_xy();//Global barrier
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
    {
        uint32_t vl;
        uint16_t * x = (uint16_t *)local(0x2000);
        uint16_t * y = (uint16_t *)local(0x8000);

        uint32_t avl = _AVL;
        //initialize vector
        
        for (int i = 0; i < avl; ++i)
        {
            if (i<avl/2) x[i] = 0x3800; //0.5  
            else x[i] = 0x3400; //0.25
            y[i] = 0x0000; //0.0
        }

        //log out initialization
        printf("[Initialize Vector X]\n");
        for (int i = 0; i < avl; i+=avl/8)
        {
            printf("[%d]  %f\n", i, fp16_to_float(x[i]));
        }
        printf("[Initialize Vector Y]\n");
        for (int i = 0; i < avl; i+=avl/8)
        {
            printf("[%d]  %f\n", i, fp16_to_float(y[i]));
        }

        do{
            asm volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(avl));
            asm volatile("vle16.v v0,  (%0)" ::"r"(x));
            asm volatile("vfadd.vv v8, v0, v0");
            asm volatile("vse16.v v8,  (%0)" ::"r"(y));
            avl -= vl;
            printf("[avl] %d, [vl] %d\n", avl, vl);
            // bowwang: this implementation is not correct
            //          x, y pointers need to shift
        } while (avl>0);

        // restore avl
        avl = _AVL;

        printf("[Vector Operation: Y = X + X]\n");
        for (int i = 0; i < avl; i+=1)
        {
            printf("[%d]  %f\n", i, fp16_to_float(y[i]));
        }

        asm volatile("vfmul.vv v8, v0, v0");
        asm volatile("vse16.v v8,  (%0)" ::"r"(y));
        printf("[Vector Operation: Y = X * X]\n");
        for (int i = 0; i < avl; i+=avl/8)
        {
            printf("[%d]  %f\n", i, fp16_to_float(y[i]));
        }

    }
    flex_global_barrier_xy();//Global barrier
}

#endif