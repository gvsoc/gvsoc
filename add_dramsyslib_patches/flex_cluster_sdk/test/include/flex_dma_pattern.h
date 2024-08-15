#ifndef _FLEX_DMA_PATTERN_H_
#define _FLEX_DMA_PATTERN_H_

#include "snrt.h"
#include "flex_runtime.h"
#include "flex_cluster_arch.h"

inline void flex_push_stack(){
    asm volatile (
        "addi sp, sp, -96 \n"    // Adjust stack pointer (allocate 96 bytes on stack)
        "sw ra, 92(sp) \n"       // Save return address (ra) on stack
        "sw s0, 88(sp) \n"       // Save frame pointer (s0) on stack
        "sw s1, 84(sp) \n"       // Save s1 on stack
        "sw s2, 80(sp) \n"       // Save s2 on stack
        "sw s3, 76(sp) \n"       // Save s3 on stack
        "sw s4, 72(sp) \n"       // Save s4 on stack
        "sw s5, 68(sp) \n"       // Save s5 on stack
        "sw s6, 64(sp) \n"       // Save s6 on stack
        "sw s7, 60(sp) \n"       // Save s7 on stack
        "sw s8, 56(sp) \n"       // Save s8 on stack
        "sw s9, 52(sp) \n"       // Save s9 on stack
        "sw s10, 48(sp) \n"      // Save s10 on stack
        "sw s11, 44(sp) \n"      // Save s11 on stack
        "sw t0, 40(sp) \n"       // Save t0 on stack
        "sw t1, 36(sp) \n"       // Save t1 on stack
        "sw t2, 32(sp) \n"       // Save t2 on stack
        "sw t3, 28(sp) \n"       // Save t3 on stack
        "sw t4, 24(sp) \n"       // Save t4 on stack
        "sw t5, 20(sp) \n"       // Save t5 on stack
        "sw t6, 16(sp) \n"       // Save t6 on stack
    );
}

inline void flex_pull_stack(){
    asm volatile (
        "lw ra, 92(sp) \n"       // Restore return address (ra)
        "lw s0, 88(sp) \n"       // Restore frame pointer (s0)
        "lw s1, 84(sp) \n"       // Restore s1
        "lw s2, 80(sp) \n"       // Restore s2
        "lw s3, 76(sp) \n"       // Restore s3
        "lw s4, 72(sp) \n"       // Restore s4
        "lw s5, 68(sp) \n"       // Restore s5
        "lw s6, 64(sp) \n"       // Restore s6
        "lw s7, 60(sp) \n"       // Restore s7
        "lw s8, 56(sp) \n"       // Restore s8
        "lw s9, 52(sp) \n"       // Restore s9
        "lw s10, 48(sp) \n"      // Restore s10
        "lw s11, 44(sp) \n"      // Restore s11
        "lw t0, 40(sp) \n"       // Restore t0
        "lw t1, 36(sp) \n"       // Restore t1
        "lw t2, 32(sp) \n"       // Restore t2
        "lw t3, 28(sp) \n"       // Restore t3
        "lw t4, 24(sp) \n"       // Restore t4
        "lw t5, 20(sp) \n"       // Restore t5
        "lw t6, 16(sp) \n"       // Restore t6
        "addi sp, sp, 96 \n"     // Adjust stack pointer back (deallocate 96 bytes from stack)
    );
}

//Pattern: Round Shift Right
void flex_dma_pattern_round_shift_right(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_pos(left_pos(pos),remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}

//Pattern: Round Shift Left
void flex_dma_pattern_round_shift_left(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_pos(right_pos(pos),remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}

void flex_dma_pattern_round_shift_up(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_pos(bottom_pos(pos),remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}


//Pattern All-to-One
void flex_dma_pattern_all_to_one(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_xy(0,0,remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}

//Pattern Dialog-to-Dialog
void flex_dma_pattern_dialog_to_dialog(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_xy(pos.y,pos.x,remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}

//Pattern Access West HBM
void flex_dma_pattern_access_west_hbm(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),hbm_west(pos.y,remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}

//Pattern Systolic-like Shifting
void flex_dma_pattern_systolic_shift_west_south_dual_idma(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();

    if (flex_is_dm_core())
    {
        if(pos.x == 0){
            /* clusters at west edge hbm transfer*/
            snrt_dma_start_1d(local(local_offset),hbm_west(pos.y,remote_offset), transfer_size);
        } else {
            /* clusters on-chip transfer*/
            snrt_dma_start_1d(local(local_offset),remote_pos(left_pos(pos),remote_offset), transfer_size);
        }
    }
    
    if (flex_is_dm2_core())
    {
        if (pos.y == 0)
        {
            /* clusters at south edge hbm transfer*/
            snrt_dma_start_1d(local(local_offset),hbm_south(pos.x,remote_offset), transfer_size);
        } else {
            /* clusters on-chip transfer*/
            snrt_dma_start_1d(local(local_offset),remote_pos(bottom_pos(pos),remote_offset), transfer_size);
        }
    }

    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}


//Pattern Systolic-like Shifting
void flex_dma_pattern_systolic_shift_west_south(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    flex_push_stack();

    FlexPosition pos = get_pos(flex_get_cluster_id());

    if(pos.x == 0){
        /* clusters at west edge hbm transfer*/
        snrt_dma_start_1d(local(local_offset),hbm_west(pos.y,remote_offset), transfer_size);
    } else {
        /* clusters on-chip transfer*/
        snrt_dma_start_1d(local(local_offset),remote_pos(left_pos(pos),remote_offset), transfer_size);
    }

    if (pos.y == 0)
    {
        /* clusters at south edge hbm transfer*/
        snrt_dma_start_1d(local(local_offset),hbm_south(pos.x,remote_offset), transfer_size);
    } else {
        /* clusters on-chip transfer*/
        snrt_dma_start_1d(local(local_offset),remote_pos(bottom_pos(pos),remote_offset), transfer_size);
    }

    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}

/***************************
*  Asynchronize Iterface   *
***************************/

//Pattern: Round Shift Right
void flex_dma_async_pattern_round_shift_right(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_pos(left_pos(pos),remote_offset), transfer_size); //Start iDMA
    flex_pull_stack();
}

//Pattern: Round Shift Left
void flex_dma_async_pattern_round_shift_left(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_pos(right_pos(pos),remote_offset), transfer_size); //Start iDMA
    flex_pull_stack();
}

void flex_dma_async_pattern_round_shift_up(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_pos(bottom_pos(pos),remote_offset), transfer_size); //Start iDMA
    flex_pull_stack();
}


//Pattern All-to-One
void flex_dma_async_pattern_all_to_one(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_xy(0,0,remote_offset), transfer_size); //Start iDMA
    flex_pull_stack();
}

//Pattern Dialog-to-Dialog
void flex_dma_async_pattern_dialog_to_dialog(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),remote_xy(pos.y,pos.x,remote_offset), transfer_size); //Start iDMA
    flex_pull_stack();
}

//Pattern Access West HBM
void flex_dma_async_pattern_access_west_hbm(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),hbm_west(pos.y,remote_offset), transfer_size); //Start iDMA
    flex_pull_stack();
}

//Pattern Access South HBM
void flex_dma_async_pattern_access_south_hbm(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    flex_push_stack();
    snrt_dma_start_1d(local(local_offset),hbm_south(pos.x,remote_offset), transfer_size); //Start iDMA
    flex_pull_stack();
}

//wait for idma
void flex_dma_async_wait_all(){
    flex_push_stack();
    snrt_dma_wait_all(); // Wait for iDMA Finishing
    flex_pull_stack();
}

#endif