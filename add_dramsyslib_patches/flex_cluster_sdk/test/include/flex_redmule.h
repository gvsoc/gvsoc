#ifndef _FLEX_REDMULE_H_
#define _FLEX_REDMULE_H_
#include "snrt.h"
#include "flex_cluster_arch.h"

void flex_redmule_set_M(uint32_t size){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE;
    *redmule_reg            = size;
}

void flex_redmule_set_N(uint32_t size){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + 4;
    *redmule_reg            = size;
}

void flex_redmule_set_K(uint32_t size){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + 8;
    *redmule_reg            = size;
}

void flex_redmule_set_X(uint32_t addr){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + 12;
    *redmule_reg            = addr;
}

void flex_redmule_set_Y(uint32_t addr){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + 16;
    *redmule_reg            = addr;
}

void flex_redmule_set_Z(uint32_t addr){
    uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + 20;
    *redmule_reg            = addr;
}

uint32_t flex_redmule_trigger_block(){
    volatile uint32_t * redmule_reg  = ARCH_REDMULE_REG_BASE + 32;
    *redmule_reg            = 32;
    return (*redmule_reg);
}


#endif