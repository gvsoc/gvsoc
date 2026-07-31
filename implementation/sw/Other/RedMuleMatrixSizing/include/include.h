#ifndef _REDMULE_MATRIX_SIZING_H_
#define _REDMULE_MATRIX_SIZING_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_transpose_engine.h"
#include "flex_libfp16.h"
#include "flex_libfp8.h"
#include "flex_dump.h"


void test_redmule(){
    if (flex_get_cluster_id() == 0 && flex_is_first_core())
    {
        uint32_t K;

        // K = 64;
        // for (int i = 0; i < 6; ++i)
        // {
        //     uint32_t M = 16 << i;
        //     for (int j = 0; j < 6; ++j)
        //     {
        //         uint32_t N = 16 << j;

        //         flex_redmule_config(M, K, N);
        //         flex_redmule_trigger(0,0,0,REDMULE_NONE_16);
        //         flex_redmule_wait();
        //     }
        // }

        // K = 128;
        // for (int i = 0; i < 6; ++i)
        // {
        //     uint32_t M = 16 << i;
        //     for (int j = 0; j < 6; ++j)
        //     {
        //         uint32_t N = 16 << j;

        //         flex_redmule_config(M, K, N);
        //         flex_redmule_trigger(0,0,0,REDMULE_NONE_16);
        //         flex_redmule_wait();
        //     }
        // }

        K = 512;
        for (int i = 0; i < 3; ++i)
        {
            uint32_t M = 16 << i;
            for (int j = 0; j < 3; ++j)
            {
                uint32_t N = 16 << j;

                flex_redmule_config(M, K, N);
                flex_redmule_trigger(0,0,0,REDMULE_NONE_16);
                flex_redmule_wait();
            }
        }
    }
}

#endif