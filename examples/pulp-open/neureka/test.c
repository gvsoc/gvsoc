/* 
 * Copyright (C) 2017 ETH Zurich, University of Bologna and GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 * Francesco Conti, University of Bologna & GreenWaves Technologies (f.conti@unibo.it)
 * Arpan Suravi Prasad, ETH Zurich (prasadar@iis.ee.ethz.ch)
 */

#include "pmsis.h"
#include <string.h>
#include "hal_neureka.h"
#include "stimuli/neureka_nn_config.h"
#include "stimuli/neureka_nn_infeat.h"
#include "stimuli/neureka_nn_norm_mult.h"
#include "stimuli/neureka_nn_norm_shift.h"
#include "stimuli/neureka_nn_norm_bias.h"
#include "stimuli/neureka_nn_weights.h"
#include "stimuli/neureka_nn_golden_outfeat.h"
#include "stimuli/neureka_nn_debug_outfeat.h"
#define WEIGHT_MEM_BASE 0x10500000
void pe_entry(void *arg)
{
    int job_id=-1;
    NEUREKA_BARRIER_ACQUIRE(job_id);
    NEUREKA_WRITE_REG(NEUREKA_REG_WEIGHTS_PTR,     WEIGHT_MEM_BASE);
    NEUREKA_WRITE_REG(NEUREKA_REG_INFEAT_PTR,      (neureka_infeat-INFEAT_OFFSET[0]));
    NEUREKA_WRITE_REG(NEUREKA_REG_OUTFEAT_PTR,     neureka_outfeat);
    NEUREKA_WRITE_REG(NEUREKA_REG_SCALE_PTR,       neureka_norm_mult);
    NEUREKA_WRITE_REG(NEUREKA_REG_SCALE_SHIFT_PTR, neureka_norm_shift);
    NEUREKA_WRITE_REG(NEUREKA_REG_SCALE_BIAS_PTR,  neureka_norm_bias);

    for(int j=7; j<25; j++) {
      NEUREKA_WRITE_REG(j*4, neureka_cfg[j]);
    }
    NEUREKA_WRITE_CMD(NEUREKA_COMMIT_AND_TRIGGER, NEUREKA_TRIGGER_CMD);
    
    NEUREKA_BARRIER();
    int errors = neureka_compare_int(neureka_outfeat, neureka_golden_outfeat, STIM_Y_SIZE/4);
    if(errors==0)
        printf("Test Passed!\n");
    else
        printf("Test failed with %d errors\n", errors);
    return 0;
}

void cluster_entry(void *arg)
{
    pi_cl_team_fork(1, pe_entry, 0);
}

static int test_entry()
{
    struct pi_device cluster_dev;
    struct pi_cluster_conf cl_conf;
    struct pi_cluster_task cl_task;

    pi_cluster_conf_init(&cl_conf);
    pi_open_from_conf(&cluster_dev, &cl_conf);
    if (pi_cluster_open(&cluster_dev))
    {
        return -1;
    }

    pi_cluster_send_task_to_cl(&cluster_dev, pi_cluster_task(&cl_task, cluster_entry, NULL));

    pi_cluster_close(&cluster_dev);

    return 0;
}

static void test_kickoff(void *arg)
{
    int ret = test_entry();
    pmsis_exit(ret);
}

int main()
{
    uint8_t* W        = neureka_weight;
    uint32_t* weight_start_ptr = WEIGHT_MEM_BASE; 
    memcpy(weight_start_ptr,(uint32_t*)neureka_weight,sizeof(neureka_weight)); 
    return pmsis_kickoff((void *)test_kickoff);
}