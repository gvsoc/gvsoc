#ifndef _FLEX_DMA_PATTERN_H_
#define _FLEX_DMA_PATTERN_H_

#include "flex_runtime.h"
#include "flex_cluster_arch.h"
#include <stddef.h>

/********************************************
*  iDMA Trigger Fucntions (Customized ISA)  *
********************************************/

#define OP_CUSTOM1 0b0101011
#define XDMA_FUNCT3 0b000
#define DMSRC_FUNCT7 0b0000000
#define DMDST_FUNCT7 0b0000001
#define DMCPYI_FUNCT7 0b0000010
#define DMCPYC_FUNCT7 0b0000011
#define DMSTATI_FUNCT7 0b0000100
#define DMMASK_FUNCT7 0b0000101
#define DMSTR_FUNCT7 0b0000110
#define DMREP_FUNCT7 0b0000111

typedef enum {
    COLLECTIVE_REDADD_UINT_16,
    COLLECTIVE_REDADD_INT_16,
    COLLECTIVE_REDADD_FP_16,
    COLLECTIVE_REDMAX_UINT_16,
    COLLECTIVE_REDMAX_INT_16,
    COLLECTIVE_REDMAX_FP_16
} collective_compute_format_t;

#define R_TYPE_ENCODE(funct7, rs2, rs1, funct3, rd, opcode)                    \
    ((funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | \
     (opcode))

inline uint32_t bare_dma_start_1d(uint64_t dst, uint64_t src,
                                          size_t size) {
    register uint32_t reg_dst_low asm("a0") = dst >> 0;    // 10
    register uint32_t reg_dst_high asm("a1") = dst >> 32;  // 11
    register uint32_t reg_src_low asm("a2") = src >> 0;    // 12
    register uint32_t reg_src_high asm("a3") = src >> 32;  // 13
    register uint32_t reg_size asm("a4") = size;           // 14

    // dmsrc a2, a3
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMSRC_FUNCT7, 13, 12,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_src_high), "r"(reg_src_low));

    // dmdst a0, a1
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMDST_FUNCT7, 11, 10,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_dst_high), "r"(reg_dst_low));

    // dmcpyi a0, a4, 0b00
    register uint32_t reg_txid asm("a0");  // 10
    asm volatile(".word %1\n"
                 : "=r"(reg_txid)
                 : "i"(R_TYPE_ENCODE(DMCPYI_FUNCT7, 0b00000, 14, XDMA_FUNCT3,
                                     10, OP_CUSTOM1)),
                   "r"(reg_size));

    return reg_txid;
}

inline uint32_t bare_dma_start_2d(uint64_t dst, uint64_t src,
                                                 size_t size, size_t dst_stride,
                                                 size_t src_stride,
                                                 size_t repeat) {
    register uint32_t reg_dst_low asm("a0") = dst >> 0;       // 10
    register uint32_t reg_dst_high asm("a1") = dst >> 32;     // 11
    register uint32_t reg_src_low asm("a2") = src >> 0;       // 12
    register uint32_t reg_src_high asm("a3") = src >> 32;     // 13
    register uint32_t reg_size asm("a4") = size;              // 14
    register uint32_t reg_dst_stride asm("a5") = dst_stride;  // 15
    register uint32_t reg_src_stride asm("a6") = src_stride;  // 16
    register uint32_t reg_repeat asm("a7") = repeat;          // 17

    // dmsrc a0, a1
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMSRC_FUNCT7, 13, 12,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_src_high), "r"(reg_src_low));

    // dmdst a0, a1
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMDST_FUNCT7, 11, 10,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_dst_high), "r"(reg_dst_low));

    // dmstr a5, a6
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMSTR_FUNCT7, 15, 16,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_src_stride), "r"(reg_dst_stride));

    // dmrep a7
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMREP_FUNCT7, 0, 17,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_repeat));

    // dmcpyi a0, a4, 0b10
    register uint32_t reg_txid asm("a0");  // 10
    asm volatile(".word %1\n"
                 : "=r"(reg_txid)
                 : "i"(R_TYPE_ENCODE(DMCPYI_FUNCT7, 0b00010, 14, XDMA_FUNCT3,
                                     10, OP_CUSTOM1)),
                   "r"(reg_size));

    return reg_txid;
}

inline void bare_dma_set_mask(uint16_t row_mask, uint16_t col_mask){
    uint32_t mask = col_mask;
    mask = (mask << 16) | row_mask;
    register uint32_t reg_mask asm("a5") = mask;           // 15
    //dmmask a5, a5
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMMASK_FUNCT7, 15, 15,
                                                  XDMA_FUNCT3, 15, OP_CUSTOM1)),
                 "r"(reg_mask), "r"(reg_mask));
}

inline uint32_t bare_dma_start_1d_broadcast(uint64_t dst, uint64_t src,
                            size_t size, uint16_t row_mask, uint16_t col_mask) {
    register uint32_t reg_dst_low asm("a0") = dst >> 0;    // 10
    register uint32_t reg_dst_high asm("a1") = dst >> 32;  // 11
    register uint32_t reg_src_low asm("a2") = src >> 0;    // 12
    register uint32_t reg_src_high asm("a3") = src >> 32;  // 13
    register uint32_t reg_size asm("a4") = size;           // 14

    bare_dma_set_mask(row_mask, col_mask);

    // dmsrc a2, a3
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMSRC_FUNCT7, 13, 12,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_src_high), "r"(reg_src_low));

    // dmdst a0, a1
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMDST_FUNCT7, 11, 10,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_dst_high), "r"(reg_dst_low));

    // dmcpyi a0, a4, 0b00
    register uint32_t reg_txid asm("a0");  // 10
    asm volatile(".word %1\n"
                 : "=r"(reg_txid)
                 : "i"(R_TYPE_ENCODE(DMCPYC_FUNCT7, 0b00001, 14, XDMA_FUNCT3,
                                     10, OP_CUSTOM1)),
                   "r"(reg_size));

    return reg_txid;
}

inline uint32_t bare_dma_start_1d_reduction(uint64_t dst, uint64_t src,
        size_t size, collective_compute_format_t fmt, uint16_t row_mask, uint16_t col_mask) {
    register uint32_t reg_dst_low asm("a0") = dst >> 0;    // 10
    register uint32_t reg_dst_high asm("a1") = dst >> 32;  // 11
    register uint32_t reg_src_low asm("a2") = src >> 0;    // 12
    register uint32_t reg_src_high asm("a3") = src >> 32;  // 13
    register uint32_t reg_size asm("a4") = size;           // 14

    bare_dma_set_mask(row_mask, col_mask);

    // dmsrc a2, a3
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMSRC_FUNCT7, 13, 12,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_src_high), "r"(reg_src_low));

    // dmdst a0, a1
    asm volatile(".word %0\n" ::"i"(R_TYPE_ENCODE(DMDST_FUNCT7, 11, 10,
                                                  XDMA_FUNCT3, 0, OP_CUSTOM1)),
                 "r"(reg_dst_high), "r"(reg_dst_low));

    // dmcpyi a0, a4, 0b00
    register uint32_t reg_txid asm("a0");  // 10
    if (fmt == COLLECTIVE_REDADD_UINT_16)
    {
        asm volatile(".word %1\n"
                     : "=r"(reg_txid)
                     : "i"(R_TYPE_ENCODE(DMCPYC_FUNCT7, 0b00010, 14, XDMA_FUNCT3,
                                         10, OP_CUSTOM1)),
                       "r"(reg_size));
    } else if (fmt == COLLECTIVE_REDADD_INT_16){
        asm volatile(".word %1\n"
                     : "=r"(reg_txid)
                     : "i"(R_TYPE_ENCODE(DMCPYC_FUNCT7, 0b00011, 14, XDMA_FUNCT3,
                                         10, OP_CUSTOM1)),
                       "r"(reg_size));
    } else if (fmt == COLLECTIVE_REDADD_FP_16){
        asm volatile(".word %1\n"
                     : "=r"(reg_txid)
                     : "i"(R_TYPE_ENCODE(DMCPYC_FUNCT7, 0b00100, 14, XDMA_FUNCT3,
                                         10, OP_CUSTOM1)),
                       "r"(reg_size));
    } else if (fmt == COLLECTIVE_REDMAX_UINT_16){
        asm volatile(".word %1\n"
                     : "=r"(reg_txid)
                     : "i"(R_TYPE_ENCODE(DMCPYC_FUNCT7, 0b00101, 14, XDMA_FUNCT3,
                                         10, OP_CUSTOM1)),
                       "r"(reg_size));
    } else if (fmt == COLLECTIVE_REDMAX_INT_16){
        asm volatile(".word %1\n"
                     : "=r"(reg_txid)
                     : "i"(R_TYPE_ENCODE(DMCPYC_FUNCT7, 0b00110, 14, XDMA_FUNCT3,
                                         10, OP_CUSTOM1)),
                       "r"(reg_size));
    } else if (fmt == COLLECTIVE_REDMAX_FP_16){
        asm volatile(".word %1\n"
                     : "=r"(reg_txid)
                     : "i"(R_TYPE_ENCODE(DMCPYC_FUNCT7, 0b00111, 14, XDMA_FUNCT3,
                                         10, OP_CUSTOM1)),
                       "r"(reg_size));
    }

    return reg_txid;
}

inline void bare_dma_wait_all() {
    // dmstati t0, 2  # 2=status.busy
    asm volatile(
        "1: \n"
        ".word %0\n"
        "bne t0, zero, 1b \n" ::"i"(
            R_TYPE_ENCODE(DMSTATI_FUNCT7, 0b10, 0, XDMA_FUNCT3, 5, OP_CUSTOM1))
        : "t0");
}


/*************************************
*  Basic Asynchronize DMA Interface  *
*************************************/

//Basic DMA 1d transfter
void flex_dma_async_1d(uint64_t dst_addr, uint64_t src_addr, size_t transfer_size){
    bare_dma_start_1d(dst_addr, src_addr, transfer_size); //Start iDMA
}

//Basic DMA 2d transfter
void flex_dma_async_2d(uint64_t dst_addr, uint64_t src_addr, size_t transfer_size, size_t dst_stride, size_t src_stride, size_t repeat){
    bare_dma_start_2d(dst_addr, src_addr, transfer_size, dst_stride, src_stride, repeat); //Start iDMA
}

//Basic collective primitives
void flex_dma_async_broadcast(uint64_t dst_offset, uint64_t src_offset, size_t transfer_size, uint16_t row_mask, uint16_t col_mask){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d_broadcast(remote_pos(pos,dst_offset), local(src_offset), transfer_size, row_mask, col_mask); //Start iDMA
}

void flex_dma_async_reduction(uint64_t dst_offset, uint64_t src_offset, size_t transfer_size, collective_compute_format_t fmt, uint16_t row_mask, uint16_t col_mask){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d_reduction(local(dst_offset), remote_pos(pos,src_offset), transfer_size, fmt, row_mask, col_mask); //Start iDMA
}

//wait for idma
void flex_dma_async_wait_all(){
    bare_dma_wait_all(); // Wait for iDMA Finishing
}

/*************************************
*  Basic Synchronize DMA Interface   *
*************************************/

//Basic DMA 2d transfter
void flex_dma_sync_2d(uint64_t dst_addr, uint64_t src_addr, size_t transfer_size, size_t dst_stride, size_t src_stride, size_t repeat){
    bare_dma_start_2d(dst_addr, src_addr, transfer_size, dst_stride, src_stride, repeat); //Start iDMA
    bare_dma_wait_all(); // Wait for iDMA Finishing
}

/****************************************
*  Versitle Asynchronize DMA Functions  *
****************************************/


void flex_dma_async_Load_HBM_1d(uint32_t local_offset, uint32_t hbm_offset, size_t transfer_size){
    bare_dma_start_1d(local(local_offset),hbm_addr(hbm_offset), transfer_size); //Start iDMA
}

//Basic DMA 1d transfter store to HBM
void flex_dma_async_Store_HBM_1d(uint32_t local_offset, uint32_t hbm_offset, size_t transfer_size){
    bare_dma_start_1d(hbm_addr(hbm_offset), local(local_offset), transfer_size); //Start iDMA
}

/*******************************************
*  Traffic Pattern: Asynchronize Interface *
*******************************************/

//Pattern: Round Shift Right
void flex_dma_async_pattern_round_shift_right(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_pos(left_pos(pos),remote_offset), transfer_size); //Start iDMA
}

//Pattern: Round Shift Left
void flex_dma_async_pattern_round_shift_left(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_pos(right_pos(pos),remote_offset), transfer_size); //Start iDMA
}

void flex_dma_async_pattern_round_shift_up(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_pos(bottom_pos(pos),remote_offset), transfer_size); //Start iDMA
}


//Pattern All-to-One
void flex_dma_async_pattern_all_to_one(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_xy(0,0,remote_offset), transfer_size); //Start iDMA
}

//Pattern Dialog-to-Dialog
void flex_dma_async_pattern_dialog_to_dialog(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_xy(pos.y,pos.x,remote_offset), transfer_size); //Start iDMA
}

//Pattern Access West HBM
void flex_dma_async_pattern_access_west_hbm(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),hbm_west(pos.y,remote_offset), transfer_size); //Start iDMA
}

//Pattern Access South HBM
void flex_dma_async_pattern_access_south_hbm(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),hbm_south(pos.x,remote_offset), transfer_size); //Start iDMA
}

/******************************************
*  Traffic Pattern: Synchronize Interface *
******************************************/

//Pattern: Round Shift Right
void flex_dma_pattern_round_shift_right(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_pos(left_pos(pos),remote_offset), transfer_size); //Start iDMA
    bare_dma_wait_all(); // Wait for iDMA Finishing
}

//Pattern: Round Shift Left
void flex_dma_pattern_round_shift_left(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_pos(right_pos(pos),remote_offset), transfer_size); //Start iDMA
    bare_dma_wait_all(); // Wait for iDMA Finishing
}

void flex_dma_pattern_round_shift_up(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_pos(bottom_pos(pos),remote_offset), transfer_size); //Start iDMA
    bare_dma_wait_all(); // Wait for iDMA Finishing
}


//Pattern All-to-One
void flex_dma_pattern_all_to_one(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_xy(0,0,remote_offset), transfer_size); //Start iDMA
    bare_dma_wait_all(); // Wait for iDMA Finishing
}

//Pattern Dialog-to-Dialog
void flex_dma_pattern_dialog_to_dialog(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),remote_xy(pos.y,pos.x,remote_offset), transfer_size); //Start iDMA
    bare_dma_wait_all(); // Wait for iDMA Finishing
}

//Pattern Access West HBM
void flex_dma_pattern_access_west_hbm(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    bare_dma_start_1d(local(local_offset),hbm_west(pos.y,remote_offset), transfer_size); //Start iDMA
    bare_dma_wait_all(); // Wait for iDMA Finishing
}


//Pattern Systolic-like Shifting
void flex_dma_pattern_systolic_shift_west_south(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){

    FlexPosition pos = get_pos(flex_get_cluster_id());

    if(pos.x == 0){
        /* clusters at west edge hbm transfer*/
        bare_dma_start_1d(local(local_offset),hbm_west(pos.y,remote_offset), transfer_size);
    } else {
        /* clusters on-chip transfer*/
        bare_dma_start_1d(local(local_offset),remote_pos(left_pos(pos),remote_offset), transfer_size);
    }

    if (pos.y == 0)
    {
        /* clusters at south edge hbm transfer*/
        bare_dma_start_1d(local(local_offset),hbm_south(pos.x,remote_offset), transfer_size);
    } else {
        /* clusters on-chip transfer*/
        bare_dma_start_1d(local(local_offset),remote_pos(bottom_pos(pos),remote_offset), transfer_size);
    }

    bare_dma_wait_all(); // Wait for iDMA Finishing
}

#endif