#ifndef _FLEX_RUNTIME_H_
#define _FLEX_RUNTIME_H_
#include "snrt.h"
#include "flex_cluster_arch.h"

#define ARCH_NUM_CLUSTER            (ARCH_NUM_CLUSTER_X*ARCH_NUM_CLUSTER_Y)
#define cluster_index(x,y)          ((y)*ARCH_NUM_CLUSTER_X+(x))
#define local(offset)               (ARCH_CLUSTER_TCDM_BASE+offset)
#define remote_cid(cid,offset)      (ARCH_CLUSTER_TCDM_REMOTE+cid*ARCH_CLUSTER_TCDM_SIZE+offset)
#define remote_xy(x,y,offset)       (ARCH_CLUSTER_TCDM_REMOTE+cluster_index(x,y)*ARCH_CLUSTER_TCDM_SIZE+offset)
#define remote_pos(pos,offset)      (ARCH_CLUSTER_TCDM_REMOTE+cluster_index(pos.x,pos.y)*ARCH_CLUSTER_TCDM_SIZE+offset)
#define hbm_addr(offset)            (ARCH_HBM_START_BASE+offset)
#define hbm_west(hid,offset)        (ARCH_HBM_START_BASE+(hid)*ARCH_HBM_NODE_INTERLEAVE+offset)
#define hbm_north(hid,offset)       (ARCH_HBM_START_BASE+(hid)*ARCH_HBM_NODE_INTERLEAVE+ARCH_HBM_NODE_INTERLEAVE*ARCH_NUM_CLUSTER_Y+offset)
#define hbm_east(hid,offset)        (ARCH_HBM_START_BASE+(hid)*ARCH_HBM_NODE_INTERLEAVE+ARCH_HBM_NODE_INTERLEAVE*(ARCH_NUM_CLUSTER_Y+ARCH_NUM_CLUSTER_X)+offset)
#define hbm_south(hid,offset)       (ARCH_HBM_START_BASE+(hid)*ARCH_HBM_NODE_INTERLEAVE+ARCH_HBM_NODE_INTERLEAVE*2*ARCH_NUM_CLUSTER_Y+ARCH_HBM_NODE_INTERLEAVE*ARCH_NUM_CLUSTER_X+offset)

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

/*******************
*  Core Position   *
*******************/

uint32_t flex_is_dm_core(){
    uint32_t hartid;
    asm("csrr %0, mhartid" : "=r"(hartid));
    return (hartid == ARCH_NUM_CORE_PER_CLUSTER-1);
}

uint32_t flex_is_dm2_core(){
    uint32_t hartid;
    asm("csrr %0, mhartid" : "=r"(hartid));
    return (hartid == ARCH_NUM_CORE_PER_CLUSTER-2);
}

uint32_t flex_is_first_core(){
    uint32_t hartid;
    asm("csrr %0, mhartid" : "=r"(hartid));
    return (hartid == 0);
}

/*******************
*  Global Barrier  *
*******************/

uint32_t flex_get_barrier_amo_value(){
    uint32_t * amo_reg      = ARCH_CLUSTER_REG_BASE+4;
    return *amo_reg;
}

uint32_t flex_get_barrier_num_cluster(){
    uint32_t * amo_reg      = ARCH_CLUSTER_REG_BASE+8;
    return *amo_reg;
}

uint32_t flex_get_barrier_num_cluster_x(){
    uint32_t * amo_reg      = ARCH_CLUSTER_REG_BASE+12;
    return *amo_reg;
}

uint32_t flex_get_barrier_num_cluster_y(){
    uint32_t * amo_reg      = ARCH_CLUSTER_REG_BASE+16;
    return *amo_reg;
}

void flex_reset_barrier(uint32_t* barrier){
    //Note: the floonoc model can not forward atomic correctly, here
    //      we use a specific memory to do synchronization:
    //          when read  x    => return x & x=x+1
    //          when write x    => x = 0
    *barrier = 0;
}

uint32_t flex_amo_fetch_add(uint32_t* barrier){
    //Note: the floonoc model can not forward atomic correctly, here
    //      we use a specific memory to do synchronization:
    //          when read  x    => return x & x=x+1
    //          when write x    => x = 0
    return (*barrier);
}

void flex_amo_add(uint32_t* barrier){
    //Note: the floonoc model can not forward atomic correctly, here
    //      we use a specific memory to do synchronization:
    //      location must >= ARCH_SYNC_SPECIAL_MEM
    //          when write x 0  => x = 0
    //          when write x m  => x = x + 1
    *barrier = 1;
}

void flex_barrier_init(){
    uint32_t * barrier      = ARCH_SYNC_BASE;
    uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    if (flex_is_dm_core()){
        if (flex_get_cluster_id() == 0)
        {
            // __atomic_store_n(barrier, 0, __ATOMIC_RELAXED);
            flex_reset_barrier(barrier);
            *wakeup_reg = 1;
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

void flex_global_barrier(){
    uint32_t * barrier      = ARCH_SYNC_BASE;
    uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    snrt_cluster_hw_barrier();

    if (flex_is_dm_core()){
        if ((flex_get_barrier_num_cluster() - 1) == flex_amo_fetch_add(barrier)) {
            flex_reset_barrier(barrier);
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
    uint32_t * barrier_y    = ARCH_SYNC_BASE+(cluster_index(pos_x_middel,pos_y_middel)*ARCH_SYNC_INTERLEAVE)+16;
    uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    if (flex_is_dm_core()){
        if (flex_get_cluster_id() == 0)
        {
            flex_reset_barrier(barrier_y);
            for (int i = 0; i < ARCH_NUM_CLUSTER_Y; ++i)
            {
                uint32_t * barrier_x = ARCH_SYNC_BASE+(cluster_index(pos_x_middel,i)*ARCH_SYNC_INTERLEAVE)+8;
                flex_reset_barrier(barrier_x);
            }
            *wakeup_reg = 1;
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

void flex_global_barrier_xy(){

    snrt_cluster_hw_barrier();

    if (flex_is_dm_core()){

        FlexPosition pos        = get_pos(flex_get_cluster_id());
        uint32_t   pos_x_middel = (flex_get_barrier_num_cluster_x())/2;
        uint32_t   pos_y_middel = (flex_get_barrier_num_cluster_y())/2;
        uint32_t * barrier_x    = ARCH_SYNC_BASE+(cluster_index(pos_x_middel,pos.y       )*ARCH_SYNC_INTERLEAVE)+8;
        uint32_t * barrier_y    = ARCH_SYNC_BASE+(cluster_index(pos_x_middel,pos_y_middel)*ARCH_SYNC_INTERLEAVE)+16;
        uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
        uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

        //First Barrier X
        if ((flex_get_barrier_num_cluster_x() - 1) == flex_amo_fetch_add(barrier_x)) {
            flex_reset_barrier(barrier_x);

            //For cluster synced X, then sync Y
            if ((flex_get_barrier_num_cluster_y() - 1) == flex_amo_fetch_add(barrier_y))
            {
                flex_reset_barrier(barrier_y);
                *wakeup_reg = 1;
            }
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

void flex_barrier_neighbor_init(){
    volatile uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    volatile uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    if (flex_is_dm_core()){
        if (flex_get_cluster_id() == 0)
        {
            for (int x = 0; x < ARCH_NUM_CLUSTER_X; ++x)
            {
                for (int y = 0; y < ARCH_NUM_CLUSTER_Y; ++y)
                {
                    volatile uint32_t * barrier_west    = ARCH_SYNC_BASE+(cluster_index(x,y)*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+0;
                    volatile uint32_t * barrier_north   = ARCH_SYNC_BASE+(cluster_index(x,y)*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+4;
                    volatile uint32_t * barrier_east    = ARCH_SYNC_BASE+(cluster_index(x,y)*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+8;
                    volatile uint32_t * barrier_south   = ARCH_SYNC_BASE+(cluster_index(x,y)*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+12;
                    flex_reset_barrier(barrier_west);
                    flex_reset_barrier(barrier_north);
                    flex_reset_barrier(barrier_east);
                    flex_reset_barrier(barrier_south);
                }
            }
            *wakeup_reg = 1;
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

void flex_neighbor_barrier(){

    snrt_cluster_hw_barrier();

    if (flex_is_dm_core()){

        uint32_t cluster_id = flex_get_cluster_id();
        FlexPosition pos = get_pos(cluster_id);
        volatile uint32_t * west_neighbor   = ARCH_SYNC_BASE+(cluster_index(pos.x - 1,pos.y)*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+0;
        volatile uint32_t * north_neighbor  = ARCH_SYNC_BASE+(cluster_index(pos.x,pos.y + 1)*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+4;
        volatile uint32_t * east_neighbor   = ARCH_SYNC_BASE+(cluster_index(pos.x + 1,pos.y)*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+8;
        volatile uint32_t * south_neighbor  = ARCH_SYNC_BASE+(cluster_index(pos.x,pos.y - 1)*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+12;
        volatile uint32_t * check_west      = ARCH_SYNC_BASE+(cluster_id*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+8;
        volatile uint32_t * check_north     = ARCH_SYNC_BASE+(cluster_id*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+12;
        volatile uint32_t * check_east      = ARCH_SYNC_BASE+(cluster_id*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+0;
        volatile uint32_t * check_south     = ARCH_SYNC_BASE+(cluster_id*ARCH_SYNC_INTERLEAVE)+ARCH_SYNC_SPECIAL_MEM+4;

        /********************
        *   set barrier     *
        ********************/

        //west neighbor
        if (pos.x != 0)
        {
            while(*west_neighbor != 0);
            flex_amo_add(west_neighbor);
        }

        //north neighbor
        if (pos.y != ARCH_NUM_CLUSTER_Y - 1)
        {
            while(*north_neighbor != 0);
            flex_amo_add(north_neighbor);
        }

        //east neighbor
        if (pos.x != ARCH_NUM_CLUSTER_X - 1)
        {
            while(*east_neighbor != 0);
            flex_amo_add(east_neighbor);
        }

        //south neighbor
        if (pos.y != 0)
        {
            while(*south_neighbor != 0);
            flex_amo_add(south_neighbor);
        }

        /********************
        *  release barrier  *
        ********************/

        //west
        if (pos.x != 0)
        {
            while(*check_west == 0);
            flex_reset_barrier(check_west);
        }

        //north
        if (pos.y != ARCH_NUM_CLUSTER_Y - 1)
        {
            
            while(*check_north == 0);
            flex_reset_barrier(check_north);
        }

        //east
        if (pos.x != ARCH_NUM_CLUSTER_X - 1)
        {
            while(*check_east == 0);
            flex_reset_barrier(check_east);
        }

        //south
        if (pos.y != 0)
        {
            while(*check_south == 0);
            flex_reset_barrier(check_south);
        }

    }

    snrt_cluster_hw_barrier();
}

/*******************
*        EoC       *
*******************/

void flex_eoc(uint32_t val){
    volatile uint32_t * eoc_reg = ARCH_SOC_REGISTER_EOC;
    *eoc_reg = val;
}

/*******************
*   Perf Counter   *
*******************/

void flex_timer_start(){
    volatile uint32_t * start_reg    = ARCH_SOC_REGISTER_EOC + 8;
    volatile uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    volatile uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    if (flex_is_dm_core()){
        if (flex_get_cluster_id() == 0)
        {
            *start_reg = 1;
            *wakeup_reg = 1;
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

void flex_timer_end(){
    volatile uint32_t * end_reg = ARCH_SOC_REGISTER_EOC + 12;
    volatile uint32_t * wakeup_reg   = ARCH_SOC_REGISTER_WAKEUP;
    volatile uint32_t * cluster_reg  = ARCH_CLUSTER_REG_BASE;

    if (flex_is_dm_core()){
        if (flex_get_cluster_id() == 0)
        {
            *end_reg = 1;
            *wakeup_reg = 1;
        }
        *cluster_reg = 1;
    }

    snrt_cluster_hw_barrier();
}

/*******************
*      Logging     *
*******************/

void flex_log(uint32_t data){
    volatile uint32_t * log_reg = (volatile uint32_t *)(ARCH_SOC_REGISTER_EOC + 16);
    *log_reg = data;
}

#endif