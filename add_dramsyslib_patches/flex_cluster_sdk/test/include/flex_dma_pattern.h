#ifndef _FLEX_DMA_PATTERN_H_
#define _FLEX_DMA_PATTERN_H_

#include "snrt.h"
#include "flex_runtime.h"
#include "flex_cluster_arch.h"

//Pattern: Round Shift Right
//  c-->c-->c-->c
//   <----------
//
//  c-->c-->c-->c
//   <----------
//
//  c-->c-->c-->c
//   <----------
//
//  c-->c-->c-->c
//   <----------
void flex_dma_pattern_round_shift_right(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    snrt_dma_start_1d(local(local_offset),remote_pos(left_pos(pos),remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
}

//Pattern: Round Shift Left
//  c<--c<--c<--c
//  ----------->
//
//  c<--c<--c<--c
//  ----------->
//
//  c<--c<--c<--c
//  ----------->
//
//  c<--c<--c<--c
//  ----------->
//
void flex_dma_pattern_round_shift_left(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    snrt_dma_start_1d(local(local_offset),remote_pos(right_pos(pos),remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
}

//Pattern All-to-One
void flex_dma_pattern_all_to_one(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    snrt_dma_start_1d(local(local_offset),remote_xy(0,0,remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
}

//Pattern Dialog-to-Dialog
void flex_dma_pattern_dialog_to_dialog(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    snrt_dma_start_1d(local(local_offset),remote_xy(pos.y,pos.x,remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
}

//Pattern Access West HBM
void flex_dma_pattern_access_west_hbm(uint32_t local_offset, uint32_t remote_offset, size_t transfer_size){
    FlexPosition pos = get_pos(flex_get_cluster_id());
    snrt_dma_start_1d(local(local_offset),hbm_west(pos.y,remote_offset), transfer_size); //Start iDMA
    snrt_dma_wait_all(); // Wait for iDMA Finishing
}

#endif