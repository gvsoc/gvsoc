#ifndef _HELLO_WORLD_H_
#define _HELLO_WORLD_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"

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

void test_dma_broadcat_rowwise(){
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
        flex_dma_async_1d_broadcast(remote_pos(left_pos(pos),0), local(0), 64);
        // flex_dma_async_1d(remote_pos(left_pos(pos),0), local(0), 4096);
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
        flex_dma_async_1d_reduction(remote_pos(left_pos(pos),0), local(0), 64, COLLECTIVE_INT_16);
        // flex_dma_async_1d(remote_pos(left_pos(pos),0), local(0), 4096);
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
#endif