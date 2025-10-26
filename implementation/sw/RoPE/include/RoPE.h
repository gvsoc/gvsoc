#ifndef _ROPE_H_
#define _ROPE_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_libfp8.h"
#include "flex_libfp16.h"
#include "rope.h"

typedef struct RoPEInfo
{
    uint64_t        input_address;
    uint64_t        output_address;
    uint64_t        costab_address;
    uint64_t        sintab_address;
    uint64_t        position_address;
    uint32_t        num_total_token;
    uint32_t        token_embedded_length;
    uint32_t        start_token;
    uint32_t        L1_I;
    uint32_t        L1_C;
    uint32_t        L1_S;
    uint32_t        L1_P;
    uint32_t        L1_Area;
    uint32_t        L1_I_size;
    uint32_t        L1_CS_size;

    //Spatz Information
    uint32_t        spatz_num;
    uint32_t        spatz_check_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t        spatz_sid_list[ARCH_NUM_CORE_PER_CLUSTER];
    uint32_t        spatz_attached;
    uint32_t        spatz_sid;

    //L1 location for spatz partition
    uint32_t        L1SP_I;
    uint32_t        L1SP_C;
    uint32_t        L1SP_S;


}RoPEInfo;

RoPEInfo RoPEAnaylze(
    uint32_t num_total_token,
    uint32_t token_embedded_length,
    uint64_t input_address,
    uint64_t output_address,
    uint64_t costab_address,
    uint64_t sintab_address,
    uint64_t position_address)
{
    RoPEInfo info;

    info.input_address          = input_address;
    info.output_address         = output_address;
    info.costab_address         = costab_address;
    info.sintab_address         = sintab_address;
    info.position_address       = position_address;
    info.token_embedded_length  = token_embedded_length;
    info.num_total_token        = num_total_token;
    FlexPosition pos            = get_pos(flex_get_cluster_id());
    info.start_token            = ((pos.x + pos.y) % ARCH_NUM_CLUSTER_X) * ARCH_NUM_CLUSTER_Y + pos.y;
    info.L1_I                   = local(0);
    info.L1_C                   = info.L1_I + DATA_TYPE_BYTE * info.token_embedded_length;
    info.L1_S                   = info.L1_C + DATA_TYPE_BYTE * (info.token_embedded_length / 2);
    info.L1_P                   = info.L1_S + DATA_TYPE_BYTE * (info.token_embedded_length / 2);
    info.L1_Area                = info.L1_P + DATA_TYPE_BYTE;
    info.L1_I_size              = DATA_TYPE_BYTE * info.token_embedded_length;
    info.L1_CS_size             = DATA_TYPE_BYTE * (info.token_embedded_length / 2);

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
    info.L1SP_C                 = info.L1_C  + info.spatz_sid * (info.L1_CS_size / ARCH_SPATZ_ATTACED_CORES);
    info.L1SP_S                 = info.L1_S  + info.spatz_sid * (info.L1_CS_size / ARCH_SPATZ_ATTACED_CORES);

    return info;
}

inline void vector_lib_rope(uint32_t i_addr, uint32_t o_addr, uint32_t c_addr, uint32_t s_addr, uint32_t vlen_whole);

void RoPERun(RoPEInfo * info)
{
    volatile uint32_t * position_prt = (uint32_t *)info->L1_P;
    while (info->start_token < info->num_total_token)
    {
        //Load Input token and positioned cosine & sin table to L1
        if (flex_is_dm_core())
        {
#if ROPE_VIEW_ENABLE == 1
            flex_dma_async_2d(
                info->L1_I, /*destination*/
                info->input_address + info->start_token * ROPE_VIEW_N * DATA_TYPE_BYTE, /*source*/
                ROPE_VIEW_N * DATA_TYPE_BYTE, /*transfer size*/
                ROPE_VIEW_N * DATA_TYPE_BYTE, /*destination stride*/
                info->num_total_token * ROPE_VIEW_N * DATA_TYPE_BYTE, /*source stride*/
                info->token_embedded_length / ROPE_VIEW_N/*repeat*/); //Start 2D iDMA
#else
            flex_dma_async_1d(
                info->L1_I, /*destination*/
                info->input_address + info->start_token * info->L1_I_size, /*source*/
                info->L1_I_size/*transfer size*/); //Start 1D iDMA
#endif
            flex_dma_async_1d(
                info->L1_P, /*destination*/
                info->position_address + info->start_token * sizeof(uint32_t), /*source*/
                sizeof(uint32_t)/*transfer size*/); //Start 1D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
            uint32_t position = *position_prt;
            flex_dma_async_1d(
                info->L1_C, /*destination*/
                info->costab_address + position * info->L1_CS_size, /*source*/
                info->L1_CS_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_1d(
                info->L1_S, /*destination*/
                info->sintab_address + position * info->L1_CS_size, /*source*/
                info->L1_CS_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
        flex_intra_cluster_sync();

        if (info->spatz_attached)
        {
            //Compute RoPE
            vector_lib_rope(
                info->L1SP_I/*i_addr*/,
                info->L1SP_I/*o_addr*/,
                info->L1SP_C/*c_addr*/,
                info->L1SP_S/*s_addr*/,
                info->token_embedded_length / ARCH_SPATZ_ATTACED_CORES/*vlen*/);
        }
        flex_intra_cluster_sync();

        //Store back to HBM
        if (flex_is_dm_core())
        {
#if ROPE_VIEW_ENABLE == 1
            flex_dma_async_2d(
                info->output_address + info->start_token * ROPE_VIEW_N * DATA_TYPE_BYTE, /*destination*/
                info->L1_I, /*source*/
                ROPE_VIEW_N * DATA_TYPE_BYTE, /*transfer size*/
                info->num_total_token * ROPE_VIEW_N * DATA_TYPE_BYTE, /*destination stride*/
                ROPE_VIEW_N * DATA_TYPE_BYTE, /*source stride*/
                info->token_embedded_length / ROPE_VIEW_N/*repeat*/); //Start 2D iDMA
#else
            flex_dma_async_1d(
                info->output_address + info->start_token * info->L1_I_size, /*destination*/
                info->L1_I, /*source*/
                info->L1_I_size/*transfer size*/); //Start 2D iDMA
#endif
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }

        //Update counter
        info->start_token += ARCH_NUM_CLUSTER;
    }
}

inline void vector_lib_rope(
    uint32_t i_addr,
    uint32_t o_addr,
    uint32_t c_addr,
    uint32_t s_addr,
    uint32_t vlen_whole)
{
    uint32_t avl;
    uint32_t vlen = vlen_whole / 2;
    while(vlen > 0){
        asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m4, ta, ma" : "=r"(avl) : "r"(vlen));
        // i[0, 2, 4, 6, ..., n-2]  -> v0   | A
        asm volatile("vlse" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0), %1" ::"r"(i_addr), "r"(DATA_TYPE_BYTE * 2));

        // i[1, 3, 5, 7, ..., n-1]  -> v4   | B
        asm volatile("vlse" XSTR(DATA_TYPE_WIDTH) ".v v4,  (%0), %1" ::"r"(i_addr + DATA_TYPE_BYTE), "r"(DATA_TYPE_BYTE * 2));

        // cosine table             -> v8   | C
        asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v  v8,  (%0)" ::"r"(c_addr));

        // sine table               -> v12  | S
        asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v  v12, (%0)" ::"r"(s_addr));

        // v0  * v8                 -> v16  | AC
        asm volatile("vfmul.vv v16, v0, v8");

        // v4  * v8                 -> v20  | BC
        asm volatile("vfmul.vv v20, v4, v8");

        // v0  * v12                -> v24  | AS
        asm volatile("vfmul.vv v24, v0, v12");

        // v4  * v12                -> v28  | BS
        asm volatile("vfmul.vv v28, v4, v12");

        // v16 - v28                -> v0   | AC - BS
        asm volatile("vfsub.vv v0,  v16, v28");

        // v24 + v20                -> v4   | AS + BC
        asm volatile("vfadd.vv v4,  v24, v20");

        // o[0, 2, 4, 6, ..., n-2]  <- v0   | P1
        asm volatile("vsse" XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0), %1" ::"r"(o_addr), "r"(DATA_TYPE_BYTE * 2));

        // o[1, 3, 5, 7, ..., n-1]  <- v4   | P2
        asm volatile("vsse" XSTR(DATA_TYPE_WIDTH) ".v v4,  (%0), %1" ::"r"(o_addr + DATA_TYPE_BYTE), "r"(DATA_TYPE_BYTE * 2));

        vlen -= avl;
        i_addr += DATA_TYPE_BYTE*avl;
        o_addr += DATA_TYPE_BYTE*avl;
        c_addr += DATA_TYPE_BYTE*avl;
        s_addr += DATA_TYPE_BYTE*avl;
    }
}

#endif