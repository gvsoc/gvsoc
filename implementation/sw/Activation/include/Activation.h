#ifndef _ACTI_H_
#define _ACTI_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_libfp8.h"
#include "flex_libfp16.h"
#include "acti.h"

typedef struct ActivationInfo
{
    uint64_t        Input;
    uint64_t        Output;
    uint32_t        num_total_token;
    uint32_t        token_embedded_length;
    uint32_t        start_token;
    uint32_t        L1_I;
    uint32_t        L1_Area;
    uint32_t        L1_I_size;

    //Spatz Information
    uint32_t        spatz_num;
    uint32_t        spatz_check_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t        spatz_sid_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t        spatz_attached;
    uint32_t        spatz_sid;

    //L1 location for spatz partition
    uint32_t        L1SP_I;
}ActivationInfo;

ActivationInfo ActivationAnaylze(
    uint32_t num_total_token,
    uint32_t token_embedded_length,
    uint64_t input_address,
    uint64_t output_address)
{
    ActivationInfo info;

    info.Input                  = input_address;
    info.Output                 = output_address;
    info.token_embedded_length  = token_embedded_length;
    info.num_total_token        = num_total_token;
    info.start_token            = flex_get_cluster_id();
    info.L1_I                   = local(0);
    info.L1_Area                = info.L1_I + DATA_TYPE_BYTE * info.token_embedded_length;
    info.L1_I_size              = DATA_TYPE_BYTE * info.token_embedded_length;

    //Spatz information
    info.spatz_num              = ARCH_SPATZ_ATTACED_CORES;
    uint32_t temp[ARCH_NUM_CORE_PER_CLUSTER] = ARCH_SPATZ_ATTACED_CHECK_LIST;
    for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i){info.spatz_check_list[i] = temp[i];}
    uint32_t sidt[ARCH_NUM_CORE_PER_CLUSTER] = ARCH_SPATZ_ATTACED_SID_LIST;
    for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i){info.spatz_sid_list[i] = sidt[i];}
    info.spatz_attached         = info.spatz_check_list[flex_get_core_id()];
    info.spatz_sid              = info.spatz_sid_list[flex_get_core_id()];

    //L1 location for spatz partition
    info.L1SP_I                 = info.L1_I  + info.spatz_sid * (info.L1_I_size  / ARCH_SPATZ_ATTACED_CORES);

    return info;
}

inline void vector_lib_sigmoid(uint32_t i_addr, uint32_t o_addr, uint32_t vlen);
inline void vector_lib_relu(uint32_t i_addr, uint32_t o_addr, uint32_t vlen);

void ActivationRun(ActivationInfo * info)
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

        //Do Activation
        if (info->spatz_attached)
        {
#if defined(ACTI_ALGO_SIGMOID)
            vector_lib_sigmoid(
                info->L1SP_I/*i_addr*/, 
                info->L1SP_I/*o_addr*/, 
                info->token_embedded_length / ARCH_SPATZ_ATTACED_CORES/*vlen*/);
#elif defined(ACTI_ALGO_RELU)
            vector_lib_relu(
                info->L1SP_I/*i_addr*/, 
                info->L1SP_I/*o_addr*/, 
                info->token_embedded_length / ARCH_SPATZ_ATTACED_CORES/*vlen*/);
#endif
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

/*Sigmoid*/
inline void vector_lib_sigmoid(
    uint32_t i_addr,
    uint32_t o_addr,
    uint32_t vlen)
{
    uint32_t avl;
#if defined(ACTI_FP16)
    acti_data_t minus_one = 0xbc00;
#elif defined(ACTI_FP8)
    acti_data_t minus_one = 0xbc;
#endif
    asm volatile("fld fa6, (%0)" ::"r"(&minus_one));
    while(vlen > 0){
        asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(vlen));
        asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(i_addr)); // x
        asm volatile("vfmul.vf v8, v8, fa6"); // -x
        asm volatile(".word %0\n"::"i"(0x32041857)) /*vfexp.vv v16, v8*/; // e^-x
        asm volatile("vfdiv.vv v0, v16, v16"); // 1
        asm volatile("vfadd.vv v16, v16, v0"); // 1 + e^-x
        asm volatile("vfdiv.vv v8, v0, v16"); // 1/(1 + e^-x)
        asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(o_addr));
        vlen -= avl;
        i_addr += DATA_TYPE_BYTE*avl;
        o_addr += DATA_TYPE_BYTE*avl;
    }
}

/*ReLU*/
inline void vector_lib_relu(
    uint32_t i_addr,
    uint32_t o_addr,
    uint32_t vlen)
{
    uint32_t avl;
    acti_data_t zero = 0;
    asm volatile("fld fa5, (%0)" ::"r"(&zero));
    while(vlen > 0){
        asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(vlen));
        asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(i_addr));
        asm volatile("vfmax.vf v8, v8, fa5");
        asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(o_addr));
        vlen -= avl;
        i_addr += DATA_TYPE_BYTE*avl;
        o_addr += DATA_TYPE_BYTE*avl;
    }
}

#endif