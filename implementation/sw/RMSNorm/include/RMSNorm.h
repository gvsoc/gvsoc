#ifndef _DSV3_RMSNORM_H_
#define _DSV3_RMSNORM_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_libfp8.h"
#include "flex_libfp16.h"
#include "norm.h"

typedef struct RMSNormInfo
{
    uint64_t        Input;
    uint64_t        Output;
    uint32_t        num_total_token;
    uint32_t        token_embedded_length;
    uint32_t        start_token;
    uint32_t        L1_I;
    uint32_t        L1_D;
    uint32_t        L1_Area;
    uint32_t        L1_I_size;


}RMSNormInfo;

RMSNormInfo Dsv3RMSNormAnaylze(
    uint32_t num_total_token,
    uint32_t token_embedded_length,
    uint64_t input_address,
    uint64_t output_address)
{
    RMSNormInfo info;

    info.Input                  = input_address;
    info.Output                 = output_address;
    info.token_embedded_length  = token_embedded_length;
    info.num_total_token        = num_total_token;
    FlexPosition pos            = get_pos(flex_get_cluster_id());
    info.start_token            = pos.x * ARCH_NUM_CLUSTER_Y + pos.y;
    info.L1_I                   = local(0);
    info.L1_D                   = info.L1_I + DATA_TYPE_BYTE * info.token_embedded_length;
    info.L1_Area                = info.L1_D + DATA_TYPE_BYTE;
    info.L1_I_size              = DATA_TYPE_BYTE * info.token_embedded_length;

    return info;
}

float div_sqrt(float number);
inline void vector_lib_sum_x2(uint32_t v_addr, uint32_t s_addr, uint32_t vlen);
inline void vector_lib_vmul_scalar(uint32_t v_addr, uint32_t s_addr, uint32_t vlen);

void Dsv3RMSNormRun(RMSNormInfo * info)
{
    while (info->start_token < info->num_total_token)
    {
        //Load token to L1
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                info->L1_I, /*destination*/
                info->Input + info->start_token * info->L1_I_size, /*source*/
                info->L1_I_size/*transfer size*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
        flex_intra_cluster_sync();

        if (flex_is_first_core())
        {
            //Compute sum(x^2)
            vector_lib_sum_x2(
                info->L1_I/*v_addr*/,
                info->L1_D/*s_addr*/,
                info->token_embedded_length/*vlen*/);

            //Compute squre root
            float fp32_i;
            float fp32_o;

            #ifdef NORM_FP16
                fp16 * d = (fp16 *)info->L1_D;
                fp32_i = fp16_to_float(*d);
                fp32_i = fp32_i / (float)(info->token_embedded_length);
                fp32_o = div_sqrt(fp32_i);
                *d = float_to_fp16(fp32_o);
            #endif

            #ifdef NORM_FP8
                fp8_e5m2 * d = (fp8_e5m2 *)info->L1_D;
                fp32_i = fp8_e5m2_to_f32(*d);
                fp32_i = fp32_i / (float)(info->token_embedded_length);
                fp32_o = div_sqrt(fp32_i);
                *d = f32_to_fp8_e5m2(fp32_o);
            #endif

            //Update token
            vector_lib_vmul_scalar(
                info->L1_I/*v_addr*/,
                info->L1_D/*s_addr*/,
                info->token_embedded_length/*vlen*/);
        }
        flex_intra_cluster_sync();

        //Store back to HBM
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                info->Output + info->start_token * info->L1_I_size, /*destination*/
                info->L1_I, /*source*/
                info->L1_I_size/*transfer size*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }

        //Update counter
        info->start_token += ARCH_NUM_CLUSTER;
    }
}

// Basic (1 / square root) function without using standard libraries
// Input: float
// Output: float

float div_sqrt(float number) {
    if (number < 0.0f) {
        // Return -1.0 to indicate an invalid (negative) input
        return -1.0f;
    }

    if (number == 0.0f || number == 1.0f) {
        return number;
    }

    float x = number;
    float guess = number / 2.0f;
    float epsilon = 0.001f; // Accuracy threshold

    // Newton-Raphson iteration
    while ((guess * guess - x > epsilon) || (x - guess * guess > epsilon)) {
        guess = (guess + x / guess) / 2.0f;
    }

    return 1.0f / guess;
}

/*Sum (x^2)*/
inline void vector_lib_sum_x2(
    uint32_t v_addr,
    uint32_t s_addr,
    uint32_t vlen)
{
    uint32_t avl;
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(1));
    asm volatile("vmv.v.i v0, 0");
    while(vlen > 0){
        asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(vlen));
        asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(v_addr));
        asm volatile("vfmul.vv v16, v8, v8");
        asm volatile("vfredusum.vs v0, v16, v0");
        vlen -= avl;
        v_addr += DATA_TYPE_BYTE*avl;
    }
    asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(1));
    asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(s_addr));
}

/*x[i]*s*/
inline void vector_lib_vmul_scalar(
    uint32_t v_addr,
    uint32_t s_addr,
    uint32_t vlen)
{
    uint32_t avl;
    asm volatile("fld fa5, (%0)" ::"r"(s_addr));
    while(vlen > 0){
        asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(vlen));
        asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(v_addr));
        asm volatile("vfmul.vf v8, v8, fa5");
        asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(v_addr));
        vlen -= avl;
        v_addr += DATA_TYPE_BYTE*avl;
    }
}

#endif