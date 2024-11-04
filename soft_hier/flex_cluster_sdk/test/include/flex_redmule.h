#ifndef _FLEX_REDMULE_H_
#define _FLEX_REDMULE_H_
#include "snrt.h"
#include "flex_cluster_arch.h"

void flex_redmule_set_M(uint32_t redmule_id, uint32_t size){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id;
    *redmule_reg            = size;
}

void flex_redmule_set_N(uint32_t redmule_id, uint32_t size){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 4;
    *redmule_reg            = size;
}

void flex_redmule_set_K(uint32_t redmule_id, uint32_t size){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 8;
    *redmule_reg            = size;
}

void flex_redmule_set_X(uint32_t redmule_id, uint32_t addr){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 12;
    *redmule_reg            = addr;
}

void flex_redmule_set_Y(uint32_t redmule_id, uint32_t addr){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 16;
    *redmule_reg            = addr;
}

void flex_redmule_set_Z(uint32_t redmule_id, uint32_t addr){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 20;
    *redmule_reg            = addr;
}

void flex_redmule_set_W(uint32_t redmule_id, uint32_t addr){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 24;
    *redmule_reg            = addr;
}

uint32_t flex_redmule_trigger_block(uint32_t redmule_id){
    volatile uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 32;
    *redmule_reg            = 32;
    return (*redmule_reg);
}

uint32_t flex_redmule_trigger_async(uint32_t redmule_id){
    volatile uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 36;
    return (*redmule_reg);
}

uint32_t flex_redmule_trigger_wait(uint32_t redmule_id){
    volatile uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + ARCH_REDMULE_REG_SIZE * redmule_id + 40;
    return (*redmule_reg);
}


#endif