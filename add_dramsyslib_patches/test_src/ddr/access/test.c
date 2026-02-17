/* 
 * Copyright (C) 2017 ETH Zurich, University of Bologna and GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 */

#include "pmsis.h"
#include "pmsis/cluster/dma/cl_dma.h"
#include "stdio.h"

#define DRAM_BASE_ADDR  0x80000000
#define DRAM_SIZE       0x40000000
#define L1_SIZE         0x10000
#define BUFF_SIZE       2048
#define COL_SIZE        32
#define NB_COPY         4
#define STRIDE          0x4000000

static char ext_buff0[BUFF_SIZE];
static char ext_buff1[BUFF_SIZE];
static char *loc_buff;

static char *dram_buff = (char*) DRAM_BASE_ADDR; // Potentially dangerous as not allocating the DRAM space

static void cluster_dram_access(void *arg)
{
  pi_cl_dma_cmd_t copy[NB_COPY];

  // Put a BUFF into L1
  for (int i=0; i<NB_COPY; i++)
  {
    pi_cl_dma_cmd((int)ext_buff0 + i*BUFF_SIZE/NB_COPY, (int)loc_buff + BUFF_SIZE/NB_COPY*i, BUFF_SIZE/NB_COPY, PI_CL_DMA_DIR_EXT2LOC, &copy[i]);
  }

  // Blocks core until transfer is finished
  for (int i=0; i<NB_COPY; i++)
  {
    pi_cl_dma_cmd_wait(&copy[i]);
  }

  // Multiply everything by 3 to check
  for (int i=0; i<BUFF_SIZE; i++)
  {
    loc_buff[i] = (char )(loc_buff[i] * 3);
  }

  // Put BUFF into DRAM for NB_COPY times with a stride of STRIDE
  for (int j=0; j<BUFF_SIZE/COL_SIZE; j++)
  {
    // Copy a BUFF-sized block
    for (int i=0; i<NB_COPY; i++)
    { 
      pi_cl_dma_cmd((int)dram_buff + COL_SIZE*j + STRIDE*i, (int)loc_buff + COL_SIZE*j, COL_SIZE, PI_CL_DMA_DIR_LOC2EXT, &copy[i]);
    }
    // Wait for transfers
    for (int i=0; i<NB_COPY; i++)
    {
      pi_cl_dma_cmd_wait(&copy[i]);
    }
  }
}

static int test_entry()
{
  printf("Entering main controller\n");

#if 1 //ef ARCHI_HAS_FC

  struct pi_device cluster_dev;
  struct pi_cluster_conf conf;
  struct pi_cluster_task cluster_task;
  struct pi_task task;

  pi_cluster_conf_init(&conf);  // Initialize a cluster configuration with default values.

  pi_open_from_conf(&cluster_dev, &conf); // Open a device
    
  pi_cluster_open(&cluster_dev);  // Configure the device and make it usable, init its handle.

  loc_buff = pmsis_l1_malloc(BUFF_SIZE);  // Allocates BUFF_SIZE bytes (4B-aligned)

  for (int i=0; i<BUFF_SIZE; i++)
  {
    ext_buff0[i] = i; // Fill ext_buff0
  }

  pi_cluster_task(&cluster_task, cluster_dram_access, NULL);  // Initializes a cluster task with the task (cluster_dram_access) before sending for execution

  pi_cluster_send_task_to_cl(&cluster_dev, &cluster_task);

  for (int j=0; j<NB_COPY; j++) // Check DRAM content
  {
    for (int i=0; i<BUFF_SIZE; i++) // Checks that the result of the task is correct
    {
      if (dram_buff[i + STRIDE*j] != (char)(i * 3)) {
        printf("ERROR at index %d / addr %p: at stride %d expecting 0x%x, got 0x%x\n", i, &dram_buff[i + STRIDE*j], j, i*3, dram_buff[i + STRIDE*j]);
        return -1;
      }
    }
  }

  pi_cluster_close(&cluster_dev);

#else

  loc_buff = pmsis_l1_malloc(BUFF_SIZE);

  for (int i=0; i<BUFF_SIZE; i++)
  {
    ext_buff0[i] = i;
  }

  cluster_entry(NULL);
  
  for (int i=0; i<BUFF_SIZE; i++)
  {
    if (ext_buff1[i] != (char)(i * 3)) {
      printf("ERROR at index %d / addr %p: expecting 0x%x, got 0x%x\n", i, &ext_buff1[i], i*3, ext_buff1[i]);
      return -1;
    }
  }

#endif

  printf("TEST SUCCESS\n");

  return 0;
}

static void test_kickoff(void *arg)
{
  int ret = test_entry();
  pmsis_exit(ret);
}

int main()
{
  return pmsis_kickoff((void *)test_kickoff);
}
