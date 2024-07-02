#ifndef _EOC_H_
#define _EOC_H_
#include "snrt.h"
#include "flex_cluster_arch.h"

uint32_t get_cluster_id(){
    uint32_t * cluster_reg      = ARCH_CLUSTER_REG_BASE;
    return *cluster_reg;
}

void flex_barrier_init(){
    uint32_t * barrier      = ARCH_SYNC_BASE;
    snrt_cluster_hw_barrier();

    if (snrt_is_dm_core()){
        __atomic_store_n(barrier, 0, __ATOMIC_RELAXED);
    }

    snrt_cluster_hw_barrier();
    snrt_cluster_hw_barrier();
    snrt_cluster_hw_barrier();
}

void flex_global_barrier(uint32_t num_clusters){
    uint32_t * barrier      = ARCH_SYNC_BASE;
    uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    uint32_t * cluster_reg      = ARCH_CLUSTER_REG_BASE;

    snrt_cluster_hw_barrier();

    if (snrt_is_dm_core()){
        if ((num_clusters - 1) == __atomic_fetch_add(barrier, 1, __ATOMIC_RELAXED)) {
            __atomic_store_n(barrier, 0, __ATOMIC_RELAXED);
            *wakeup_reg = 1;
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

void eoc(uint32_t val){
    uint32_t * eoc_reg = ARCH_SOC_REGISTER_EOC;
    *eoc_reg = val;
}

#endif