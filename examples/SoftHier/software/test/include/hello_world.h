#ifndef _HELLO_WORLD_H_
#define _HELLO_WORLD_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"

void hello_world_core0(){
    flex_global_barrier_xy();//Global barrier
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
    {
    	printf("Hello World\n");
    }
    flex_global_barrier_xy();//Global barrier
}

void hello_world_all_cluster(){
    flex_global_barrier_xy();//Global barrier
    for (int cid = 0; cid < ARCH_NUM_CLUSTER; ++cid)
    {
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == cid)
        {
            printf("[Cluster %3d] Hello World\n", cid);
        }
        flex_global_barrier_xy();//Global barrier
    }
    flex_global_barrier_xy();//Global barrier
}

void hello_world_all_core(){
    flex_global_barrier_xy();//Global barrier
    for (int cid = 0; cid < ARCH_NUM_CLUSTER; ++cid)
    {
        for (int i = 0; i < ARCH_NUM_CORE_PER_CLUSTER; ++i)
        {
            if (flex_get_core_id() == i && flex_get_cluster_id() == cid)
            {
                if (flex_get_core_id() == 0)
                {
                    printf("[Cluster %3d] ---------------------------------------\n", cid);
                }
                printf("    [Core %2d] Hello World\n", i);
            }
            flex_global_barrier_xy();//Global barrier
        }
    }
    flex_global_barrier_xy();//Global barrier
}

void test_global_barrier(){
    for (int i = 0; i < 300; ++i)
    {
        flex_global_barrier_xy();//Global barrier
    }
}

void test_global_barrier_polling(){
    // flex_global_barrier_xy_polling();
    for (int i = 0; i < 300; ++i)
    {
        flex_global_barrier_xy_polling();
    }
}

void compare_hw_sync_and_polling_sync()
{
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    test_global_barrier();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    test_global_barrier_polling();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
}

void test_group_barrier(){
    flex_global_barrier_xy();//Global barrier
    GridSyncGroupInfo info = grid_sync_group_init(2,2);
    flex_global_barrier_xy();//Global barrier
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
    {
        printf("Hello World\n");
        if (info.valid_grid == 1)
        {
            printf("[PASSED] It is a valid synchronization group\n");
        } else {
            printf("[FAILED] Set a invalid synchronization group\n");
        }
        printf("Then we analysis GridSyncGroupInfo of each cluster\n");
    }
    flex_global_barrier_xy();//Global barrier
    for (int cid = 0; cid < ARCH_NUM_CLUSTER; ++cid)
    {
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == cid)
        {
            printf("[Cluster %3d] All Info: \n", cid);
            printf("-- valid_grid = %0d \n", info.valid_grid);
            printf("-- grid_x_dim = %0d \n", info.grid_x_dim);
            printf("-- grid_y_dim = %0d \n", info.grid_y_dim);
            printf("-- grid_x_num = %0d \n", info.grid_x_num);
            printf("-- grid_y_num = %0d \n", info.grid_y_num);
            printf("-- this_grid_id = %0d \n", info.this_grid_id);
            printf("-- this_grid_id_x = %0d \n", info.this_grid_id_x);
            printf("-- this_grid_id_y = %0d \n", info.this_grid_id_y);
            printf("-- this_grid_left_most = %0d \n", info.this_grid_left_most);
            printf("-- this_grid_right_most = %0d \n", info.this_grid_right_most);
            printf("-- this_grid_top_most = %0d \n", info.this_grid_top_most);
            printf("-- this_grid_bottom_most = %0d \n", info.this_grid_bottom_most);
            printf("-- this_grid_cluster_num = %0d \n", info.this_grid_cluster_num);
            printf("-- this_grid_cluster_num_x = %0d \n", info.this_grid_cluster_num_x);
            printf("-- this_grid_cluster_num_y = %0d \n", info.this_grid_cluster_num_y);
            printf("-- wakeup_row_mask = 0x%0x \n", info.wakeup_row_mask);
            printf("-- wakeup_col_mask = 0x%0x \n", info.wakeup_col_mask);
            printf("-- sync_x_cluster = %0d \n", info.sync_x_cluster);
            printf("-- sync_y_cluster = %0d \n", info.sync_y_cluster);
            printf("-- sync_x_point = 0x%0x \n", (uint32_t)info.sync_x_point);
            printf("-- sync_x_piter = 0x%0x \n", (uint32_t)info.sync_x_piter);
            printf("-- sync_y_point = 0x%0x \n", (uint32_t)info.sync_y_point);
            printf("-- sync_y_piter = 0x%0x \n", (uint32_t)info.sync_y_piter);
        }
        flex_global_barrier_xy();//Global barrier
    }
    flex_global_barrier_xy();//Global barrier
    grid_sync_group_barrier_xy_polling(&info); //Group barrier
    grid_sync_group_barrier_xy(&info); //Group barrier
    for (int i = 0; i < 10; ++i)
    {
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
        {
            flex_redmule_config(16, 16, 16);
            flex_redmule_trigger(0, 0, 0, REDMULE_UINT_16);
            flex_redmule_wait();
        }
        grid_sync_group_barrier_xy(&info); //Group barrier
    }
}

void test_dma(){
	flex_global_barrier_xy();//Global barrier
	if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {
    	uint32_t * dram_p1 = (uint32_t *)ARCH_HBM_START_BASE;
    	//prapare data
    	for (int i = 0; i < 16; ++i)
    	{
			dram_p1[i] = i;
    	}
    	//check data
    	for (int i = 0; i < 16; ++i)
    	{
    		printf("%d\n", dram_p1[i]);
    	}

    	//move data from HBM to Cluster L1
    	flex_dma_async_1d(local(0),hbm_addr(0), 16*4);
    	flex_dma_async_wait_all();

    	//check data
    	uint32_t * l1_p = (uint32_t *)local(0);
    	for (int i = 0; i < 16; ++i)
    	{
    		printf("%d\n", l1_p[i]);
    	}

    }
    flex_global_barrier_xy();//Global barrier
}

void test_redmule(){
	flex_global_barrier_xy();//Global barrier
	if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {
    	uint16_t * x = (uint16_t *)local(0x0000);
    	uint16_t * w = (uint16_t *)local(0x1000);
    	uint16_t * z = (uint16_t *)local(0x2000);

    	//prapare data
    	for (int i = 0; i < 16; ++i)
    	{
    		x[i] = 1;
    		w[i] = 2;
    		z[i] = 1;
    	}

    	//check data
    	for (int i = 0; i < 16; ++i)
    	{
    		printf("%0d, %0d, %0d\n", x[i], w[i], z[i]);
    	}

    	//redmule
    	flex_redmule_config(4, 4, 4);
    	flex_redmule_trigger((uint32_t)x, (uint32_t)w, (uint32_t)z, REDMULE_UINT_16);
    	flex_redmule_wait();

    	//check data
    	for (int i = 0; i < 16; ++i)
    	{
    		printf("%0d, %0d, %0d\n", x[i], w[i], z[i]);
    	}
    }
    flex_global_barrier_xy();//Global barrier
}

void check_hbm_preload(){
    flex_global_barrier_xy();//Global barrier
    if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {
        int32_t * dram_p1 = (int32_t *)ARCH_HBM_START_BASE;

        //check data
        for (int i = 0; i < 2; ++i)
        {
            flex_log_char((char)(dram_p1[i] + 48));
            flex_log_char('\n');
        }

        // //move data from HBM to Cluster L1
        // flex_dma_async_1d(local(0),hbm_addr(0), 16*4);
        // flex_dma_async_wait_all();

        // flex_dma_async_1d(hbm_addr(512), local(0), 16*4);
        // flex_dma_async_wait_all();

    }
    flex_global_barrier_xy();//Global barrier
}

void zero_mem_test(){
    flex_global_barrier_xy();//Global barrier
    if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {

        //move data from HBM to Cluster L1
        flex_dma_async_1d(local(0),zomem(0), 16*4);
        flex_dma_async_wait_all();

    }
    flex_global_barrier_xy();//Global barrier
}

void test_synchronization(){
    for (int i = 0; i < 100; ++i)
    {
        flex_global_barrier_xy();
    }
}


void test_dma_2d(){
    flex_global_barrier_xy();//Global barrier
    if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {
        //move data from HBM to Cluster L1
        bare_dma_start_2d(local(0),hbm_addr(0), 64, 64, 64, 32);
        flex_dma_async_wait_all();

    }
    flex_global_barrier_xy();//Global barrier
}

void test_dma_collectives(){
    flex_global_barrier_xy();//Global barrier
    FlexPosition pos = get_pos(flex_get_cluster_id());
    if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {
        uint16_t * x = (uint16_t *)local(0x0000);

        //prapare data
        for (int i = 0; i < 16; ++i)
        {
            x[i] = 1;
        }
    }
    flex_global_barrier_xy();//Global barrier

    //check data
    for (int cid = 0; cid < 4; ++cid)
    {
        if (flex_is_dm_core() && flex_get_cluster_id() == cid)
        {
            uint16_t * x = (uint16_t *)local(0x0000);
            //check data
            printf("Data array in Cluster %0d\n", cid);
            for (int i = 0; i < 16; ++i)
            {
                printf("%0d\n", x[i]);
            }
        }
        flex_global_barrier_xy();//Global barrier
    }
    flex_global_barrier_xy();//Global barrier

    //do broadcast
    if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {
        flex_dma_async_broadcast(0, 0, 64, 0b00, 0b11);
        flex_dma_async_wait_all();

    }
    flex_global_barrier_xy();//Global barrier

    //check data
    for (int cid = 0; cid < 4; ++cid)
    {
        if (flex_is_dm_core() && flex_get_cluster_id() == cid)
        {
            uint16_t * x = (uint16_t *)local(0x0000);
            //check data
            printf("Data array in Cluster %0d\n", cid);
            for (int i = 0; i < 16; ++i)
            {
                printf("%0d\n", x[i]);
            }
        }
        flex_global_barrier_xy();//Global barrier
    }
    flex_global_barrier_xy();//Global barrier

    //do redcution
    if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {
        flex_dma_async_reduction(0, 0, 64, COLLECTIVE_REDADD_INT_16, 0b00, 0b11);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();//Global barrier

    //check data
    for (int cid = 0; cid < 4; ++cid)
    {
        if (flex_is_dm_core() && flex_get_cluster_id() == cid)
        {
            uint16_t * x = (uint16_t *)local(0x0000);
            //check data
            printf("Data array in Cluster %0d\n", cid);
            for (int i = 0; i < 16; ++i)
            {
                printf("%0d\n", x[i]);
            }
        }
        flex_global_barrier_xy();//Global barrier
    }
    flex_global_barrier_xy();//Global barrier
}

void test_HBM_interleaving(){
    flex_global_barrier_xy();//Global barrier
    if (flex_is_dm_core() && flex_get_cluster_id() == 0)
    {
        //move data from HBM to Cluster L1
        flex_dma_async_1d(local(0),hbm_addr(0), 256*256);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();//Global barrier
}

#include "flex_libfp16.h"
void test_FP16(){
    flex_global_barrier_xy();//Global barrier
    if (flex_is_first_core() && flex_get_cluster_id() == 0)
    {
        fp16 a = 0x3800;
        fp16 b = 0x3C00;
        fp16 c = 0x0000;
        printf("a           = %f\n", fp16_to_float(a));
        printf("b           = %f\n", fp16_to_float(b));
        asm_fp16_div(&a, &b, &c);
        printf("a / b       = %f\n", fp16_to_float(c));
        asm_fp16_exp(&a, &c);
        printf("exp(a)      = %f\n", fp16_to_float(c));
        asm_fp16_sigmoid(&a, &c);
        printf("sigmoid(a)  = %f\n", fp16_to_float(c));
        int cmp = asm_fp16_compare(&a, &b);
        if (cmp < 0)
        {
            printf("compare a&b : a < b\n");
        } else if (cmp == 0)
        {
            printf("compare a&b : a == b\n");
        } else {
            printf("compare a&b : a > b\n");
        }
    }
    flex_global_barrier_xy();//Global barrier
}

#define DATA_SIZE      1024
void test_malloc_single_cluster(){
    // l1 heap allocator init
    flex_alloc_init();
    flex_global_barrier_xy();//Global barrier
    flex_intra_cluster_sync();//Cluster barrier

    uint32_t CID = flex_get_cluster_id();//Get cluster ID
    uint32_t addr_a, addr_b, addr_c, addr_d;

    // test memory allocation with 1 cluster (CID==0)
    if (flex_is_first_core() && (CID == 0))//Use the first core for memory allocation
        {
            printf("\n\n\n");
            printf("Heap start addr: 0x%08x\n\n", ARCH_CLUSTER_HEAP_BASE);

            addr_a = (uint32_t)flex_l1_malloc(DATA_SIZE);
            printf("Allocating A with size 0x%x ------> addr_a: 0x%08x\n\n", DATA_SIZE, addr_a);
            flex_dump_heap();

            addr_b = (uint32_t)flex_l1_malloc(DATA_SIZE);
            printf("Allocating B with size 0x%x ------> addr_b: 0x%08x\n\n", DATA_SIZE, addr_b);
            flex_dump_heap();

            addr_c = (uint32_t)flex_l1_malloc(DATA_SIZE);
            printf("Allocating C with size 0x%x ------> addr_c: 0x%08x\n\n", DATA_SIZE, addr_c);
            flex_dump_heap();

            printf("\n\n\n");
            printf("Deallocate B\n\n");
            flex_l1_free((void *)addr_b);
            flex_dump_heap();

            // addr_d should be inseted between addr_a and addr_c
            addr_d = (uint32_t)flex_l1_malloc(DATA_SIZE/2);
            printf("Allocating D with size 0x%x ------> addr_d: 0x%08x\n\n", DATA_SIZE/2, addr_d);
            flex_dump_heap();

            // addr_e should be added after addr_c
            addr_d = (uint32_t)flex_l1_malloc(DATA_SIZE*2);
            printf("Allocating E with size 0x%x ------> addr_e: 0x%08x\n\n", DATA_SIZE*2, addr_d);
            flex_dump_heap();
        }

    flex_intra_cluster_sync();//Cluster barrier
    flex_global_barrier_xy();//Global barrier
}
#endif

