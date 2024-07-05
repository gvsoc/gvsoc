#ifndef _FLEX_RUNTIME_H_
#define _FLEX_RUNTIME_H_
#include "snrt.h"
#include "flex_cluster_arch.h"

#define ARCH_NUM_CLUSTER            (ARCH_NUM_CLUSTER_X*ARCH_NUM_CLUSTER_Y)
#define cluster_index(x,y)          (y*ARCH_NUM_CLUSTER_X+x)
#define local(offset)               (ARCH_CLUSTER_TCDM_BASE+offset)
#define remote_cid(cid,offset)      (ARCH_CLUSTER_TCDM_REMOTE+cid*ARCH_CLUSTER_TCDM_SIZE+offset)
#define remote_xy(x,y,offset)       (ARCH_CLUSTER_TCDM_REMOTE+cluster_index(x,y)*ARCH_CLUSTER_TCDM_SIZE+offset)
#define remote_pos(pos,offset)      (ARCH_CLUSTER_TCDM_REMOTE+cluster_index(pos.x,pos.y)*ARCH_CLUSTER_TCDM_SIZE+offset)

/*******************
* Cluster Position *
*******************/

typedef struct FlexPosition
{
    uint32_t x;
    uint32_t y;
}FlexPosition;

FlexPosition get_pos(uint32_t cluster_id) {
    FlexPosition pos;
    pos.x = cluster_id % ARCH_NUM_CLUSTER_X;
    pos.y = cluster_id / ARCH_NUM_CLUSTER_X;
    return pos;
}

//Methods
FlexPosition right_pos(FlexPosition pos) {
    uint32_t new_x = (pos.x + 1) % ARCH_NUM_CLUSTER_X;
    uint32_t new_y = pos.y;
    FlexPosition new_pos;
    new_pos.x = new_x;
    new_pos.y = new_y;
    return new_pos;
}

FlexPosition left_pos(FlexPosition pos) {
    uint32_t new_x = (pos.x + ARCH_NUM_CLUSTER_X - 1) % ARCH_NUM_CLUSTER_X;
    uint32_t new_y = pos.y;
    FlexPosition new_pos;
    new_pos.x = new_x;
    new_pos.y = new_y;
    return new_pos;
}

FlexPosition top_pos(FlexPosition pos) {
    uint32_t new_x = pos.x;
    uint32_t new_y = (pos.y + 1) % ARCH_NUM_CLUSTER_Y;
    FlexPosition new_pos;
    new_pos.x = new_x;
    new_pos.y = new_y;
    return new_pos;
}

FlexPosition bottom_pos(FlexPosition pos) {
    uint32_t new_x = pos.x;
    uint32_t new_y = (pos.y + ARCH_NUM_CLUSTER_Y - 1) % ARCH_NUM_CLUSTER_Y;
    FlexPosition new_pos;
    new_pos.x = new_x;
    new_pos.y = new_y;
    return new_pos;
}

uint32_t flex_get_cluster_id(){
    uint32_t * cluster_reg      = ARCH_CLUSTER_REG_BASE;
    return *cluster_reg;
}

uint32_t flex_is_dm_core(){
    uint32_t hartid;
    asm("csrr %0, mhartid" : "=r"(hartid));
    return (hartid == ARCH_NUM_CORE_PER_CLUSTER-1);
}

/*******************
*  Global Barrier  *
*******************/

uint32_t flex_get_barrier_amo_value(){
    uint32_t * amo_reg      = ARCH_CLUSTER_REG_BASE+4;
    return *amo_reg;
}

void flex_barrier_init(){
    uint32_t * barrier      = ARCH_SYNC_BASE;
    uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    snrt_cluster_hw_barrier();

    if (flex_is_dm_core()){
        if (flex_get_cluster_id() == 0)
        {
            __atomic_store_n(barrier, 0, __ATOMIC_RELAXED);
            *wakeup_reg = 1;
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
    snrt_cluster_hw_barrier();
    snrt_cluster_hw_barrier();
}

void flex_global_barrier(){
    uint32_t * barrier      = ARCH_SYNC_BASE;
    uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    snrt_cluster_hw_barrier();

    if (flex_is_dm_core()){
        if ((ARCH_NUM_CLUSTER - 1) == __atomic_fetch_add(barrier, flex_get_barrier_amo_value(), __ATOMIC_RELAXED)) {
            __atomic_store_n(barrier, 0, __ATOMIC_RELAXED);
            *wakeup_reg = 1;
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

void flex_barrier_xy_init(){
    FlexPosition pos        = get_pos(flex_get_cluster_id());
    uint32_t   pos_x_middel = (ARCH_NUM_CLUSTER_X)/2;
    uint32_t   pos_y_middel = (ARCH_NUM_CLUSTER_Y)/2;
    uint32_t * barrier_x    = ARCH_SYNC_BASE+(cluster_index(pos_x_middel,pos.y       )*ARCH_SYNC_INTERLEAVE)+8;
    uint32_t * barrier_y    = ARCH_SYNC_BASE+(cluster_index(pos_x_middel,pos_y_middel)*ARCH_SYNC_INTERLEAVE)+16;

    snrt_cluster_hw_barrier();

    if (flex_is_dm_core()){
        __atomic_store_n(barrier_x, 0, __ATOMIC_RELAXED);
        __atomic_store_n(barrier_y, 0, __ATOMIC_RELAXED);
    }

    snrt_cluster_hw_barrier();
    snrt_cluster_hw_barrier();
    snrt_cluster_hw_barrier();
}

void flex_global_barrier_xy(){
    FlexPosition pos        = get_pos(flex_get_cluster_id());
    uint32_t   pos_x_middel = (ARCH_NUM_CLUSTER_X)/2;
    uint32_t   pos_y_middel = (ARCH_NUM_CLUSTER_Y)/2;
    uint32_t * barrier_x    = ARCH_SYNC_BASE+(cluster_index(pos_x_middel,pos.y       )*ARCH_SYNC_INTERLEAVE)+8;
    uint32_t * barrier_y    = ARCH_SYNC_BASE+(cluster_index(pos_x_middel,pos_y_middel)*ARCH_SYNC_INTERLEAVE)+16;
    uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    snrt_cluster_hw_barrier();

    if (snrt_is_dm_core()){
        //First Barrier X
        if ((ARCH_NUM_CLUSTER_X - 1) == __atomic_fetch_add(barrier_x, flex_get_barrier_amo_value(), __ATOMIC_RELAXED)) {
            __atomic_store_n(barrier_x, 0, __ATOMIC_RELAXED);

            //For cluster synced X, then sync Y
            if ((ARCH_NUM_CLUSTER_Y - 1) == __atomic_fetch_add(barrier_y, flex_get_barrier_amo_value(), __ATOMIC_RELAXED))
            {
                __atomic_store_n(barrier_y, 0, __ATOMIC_RELAXED);
                *wakeup_reg = 1;
            }
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

/*******************
*        EoC       *
*******************/

void flex_eoc(uint32_t val){
    uint32_t * eoc_reg = ARCH_SOC_REGISTER_EOC;
    *eoc_reg = val;
}

#endif