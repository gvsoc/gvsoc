#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"
#include "kernels/common/common_tiling.h"
#include "kernels/llm_mlp/llm_mlp_matmul.h"
#include "kernels/llm_mlp/llm_mlp_inter_cluster_matmul.h"
#include "kernels/kernel_test_dma_redmule.h"
#include <math.h>

#define TILE_DIMENSION      256
#define NUM_ITERATION       1
#define ELEMENT_SIZE        2
#define TILE_SIZE           (TILE_DIMENSION * TILE_DIMENSION * ELEMENT_SIZE)
#define X1_ADDR             0
#define W1_ADDR             (X1_ADDR + TILE_SIZE)
#define X2_ADDR             (W1_ADDR + TILE_SIZE)
#define W2_ADDR             (X2_ADDR + TILE_SIZE)
#define YZ_ADDR             (W2_ADDR + TILE_SIZE)

int main()
{
    /*******************/
    /*  Initailization */
    /*******************/
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    //Initialize RedMule Paramters
    if (flex_is_first_core())
    {
        flex_redmule_set_M(0, TILE_DIMENSION);
        flex_redmule_set_N(0, TILE_DIMENSION);
        flex_redmule_set_K(0, TILE_DIMENSION);
        flex_redmule_set_Y(0, YZ_ADDR);
        flex_redmule_set_Z(0, YZ_ADDR);
    }

    flex_global_barrier_xy();

    /*******************/
    /*  Program Start  */
    /*******************/
    //Perf Counter Start
    flex_timer_start();

    for (int i = 0; i < NUM_ITERATION; ++i)
    {
        if (flex_is_dm_core()){
            //X tiles from west hbm channels
            flex_dma_async_pattern_access_west_hbm(X1_ADDR, 2*i*TILE_SIZE, TILE_SIZE);
            //Y tiles from south hbm channels
            flex_dma_async_pattern_access_south_hbm(W1_ADDR, 2*i*TILE_SIZE, TILE_SIZE);
            //Wait for all DMA TXN done
            flex_dma_async_wait_all();
        }

        if (flex_is_first_core())
        {
            flex_redmule_set_X(0, X2_ADDR);
            flex_redmule_set_W(0, W2_ADDR);
            flex_redmule_trigger_async(0);
            flex_redmule_trigger_wait(0);
        }

        flex_global_barrier_xy();

        if (flex_is_dm_core()){
            //X tiles from west hbm channels
            flex_dma_async_pattern_access_west_hbm(X2_ADDR, (2*i + 1)*TILE_SIZE, TILE_SIZE);
            //Y tiles from south hbm channels
            flex_dma_async_pattern_access_south_hbm(W2_ADDR, (2*i + 1)*TILE_SIZE, TILE_SIZE);
            //Wait for all DMA TXN done
            flex_dma_async_wait_all();
        }

        if (flex_is_first_core())
        {
            flex_redmule_set_X(0, X1_ADDR);
            flex_redmule_set_W(0, W1_ADDR);
            flex_redmule_trigger_async(0);
            flex_redmule_trigger_wait(0);
        }

        flex_global_barrier_xy();
    }

    /*******************/
    /*  Program Stop*  */
    /*******************/
    //Perf Counter End
    flex_timer_end();
    flex_eoc(eoc_val);
	return 0;
}