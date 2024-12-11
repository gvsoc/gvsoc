#ifndef _FLEX_DMA_PATTERN_H_
#define _FLEX_DMA_PATTERN_H_

#include "flex_runtime.h"
#include "flex_cluster_arch.h"

/********************************************
*  iDMA Trigger Fucntions (Customized ISA)  *
********************************************/

#define OP_CUSTOM1 0b0101011
#define XDMA_FUNCT3 0b000
#define DMSRC_FUNCT7 0b0000000
#define DMDST_FUNCT7 0b0000001
#define DMCPYI_FUNCT7 0b0000010
#define DMCPY_FUNCT7 0b0000011
#define DMSTATI_FUNCT7 0b0000100
#define DMSTAT_FUNCT7 0b0000101
#define DMSTR_FUNCT7 0b0000110
#define DMREP_FUNCT7 0b0000111

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

//Basic DMA 1d transfter load from HBM
void flex_dma_async_1d(uint32_t dst_addr, uint32_t src_addr, size_t transfer_size){
    bare_dma_start_1d(dst_addr, src_addr, transfer_size); //Start iDMA
}

//wait for idma
void flex_dma_async_wait_all(){
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