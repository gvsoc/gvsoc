#ifndef _EXAMPLE_ONE_CLUSTER_GEMM_H_
#define _EXAMPLE_ONE_CLUSTER_GEMM_H_

#include <math.h>
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

// GEMM M-N-K           : 1024-1024-1024
// Elem Size            : Int16
// Assumption           : Data are already tiled in HBM

#define ELEM_SIZE      2
#define GEMM_DIMENSION 1024
#define GEMM_SIZE_BYTE (GEMM_DIMENSION * GEMM_DIMENSION * ELEM_SIZE)
#define TILE_DIMENSION 256
#define TILE_SIZE_BYTE (TILE_DIMENSION * TILE_DIMENSION * ELEM_SIZE)
#define TILES_PER_DIM  (GEMM_DIMENSION/TILE_DIMENSION)

#define X_HBM_OFFSET   0
#define W_HBM_OFFSET   (X_HBM_OFFSET + GEMM_SIZE_BYTE)
#define Y_HBM_OFFSET   (W_HBM_OFFSET + GEMM_SIZE_BYTE)
#define Z_HBM_OFFSET   (Y_HBM_OFFSET + GEMM_SIZE_BYTE)

#define X_L1_OFFSET1   0
#define W_L1_OFFSET1   (X_L1_OFFSET1 + TILE_SIZE_BYTE)
#define X_L1_OFFSET2   (W_L1_OFFSET1 + TILE_SIZE_BYTE)
#define W_L1_OFFSET2   (X_L1_OFFSET2 + TILE_SIZE_BYTE)
#define YZ_L1_OFFSET   (W_L1_OFFSET2 + TILE_SIZE_BYTE)

void example_one_cluster_gemm(){
    flex_global_barrier_xy();                                                                               //Global barrier

    uint32_t CID = flex_get_cluster_id();                                                                   //Get cluster ID

                                                                                                            //Initialize RedMule Paramters
    if (flex_is_first_core() && (CID == 0))                                                                 //Use the first core in cluster 0 to configure RedMule
    {
        flex_redmule_set_M(0, TILE_DIMENSION);                                                              //Configure M-N-K of tile that RedMule will accelerate
        flex_redmule_set_N(0, TILE_DIMENSION);
        flex_redmule_set_K(0, TILE_DIMENSION);
    }

    flex_global_barrier_xy();                                                                               //Global barrier

    if (CID == 0)                                                                                           //Only let Cluster 0 to work
    {
        for (int row = 0; row < TILES_PER_DIM; ++row)                                                       //Iterate over every Z tiles
        {
            for (int col = 0; col < TILES_PER_DIM; ++col)
            {                                                                                               //Start address of tiles
                uint32_t X_hbm_addr = X_HBM_OFFSET + row * TILES_PER_DIM * TILE_SIZE_BYTE;
                uint32_t W_hbm_addr = W_HBM_OFFSET + col * TILE_SIZE_BYTE;
                uint32_t Y_hbm_addr = Y_HBM_OFFSET + row * TILES_PER_DIM * TILE_SIZE_BYTE + col * TILE_SIZE_BYTE;
                uint32_t Z_hbm_addr = Z_HBM_OFFSET + row * TILES_PER_DIM * TILE_SIZE_BYTE + col * TILE_SIZE_BYTE;


                //Preload Y,X,W tiles
                if (flex_is_dm_core()){                                                                     //Use DM core to trigger DMA transcations
                    flex_dma_async_1d(local(YZ_L1_OFFSET),hbm_addr(Y_hbm_addr), TILE_SIZE_BYTE);            //Trigger DMA transaction: move Y tile from HBM to L1
                    flex_dma_async_1d(local(X_L1_OFFSET1),hbm_addr(X_hbm_addr), TILE_SIZE_BYTE);            //Trigger DMA transaction: move X tile from HBM to L1
                    flex_dma_async_1d(local(W_L1_OFFSET1),hbm_addr(W_hbm_addr), TILE_SIZE_BYTE);            //Trigger DMA transaction: move W tile from HBM to L1
                    flex_dma_async_wait_all();                                                              //Wait all DMA transaction done     
                }
                flex_intra_cluster_sync();                                                                  //Cluster barrier

                //Compute X and W to YZ in L1 + preload next X and W
                for (int i = 0; i < (TILES_PER_DIM-1); ++i)
                {
                    X_hbm_addr += TILE_SIZE_BYTE;                                                           //Calculate next X and W tile address in HBM
                    W_hbm_addr += TILES_PER_DIM * TILE_SIZE_BYTE;
                    if (i%2 == 0)                                                                           //Ping-Pong operations on Double-Buffering X and W
                    {
                        if (flex_is_dm_core()){                                                             //Use DM core to trigger DMA transcations
                            flex_dma_async_1d(local(X_L1_OFFSET2),hbm_addr(X_hbm_addr), TILE_SIZE_BYTE);    //Trigger DMA transaction: move X tile from HBM to L1
                            flex_dma_async_1d(local(W_L1_OFFSET2),hbm_addr(W_hbm_addr), TILE_SIZE_BYTE);    //Trigger DMA transaction: move W tile from HBM to L1
                            flex_dma_async_wait_all();                                                      //Wait all DMA transaction done
                        }

                        if (flex_is_first_core())                                                           //Use the first core in cluster 0 to configure and trigger RedMule
                        {
                            flex_redmule_set_X(0, X_L1_OFFSET1);                                            //Configure tile address in L1
                            flex_redmule_set_W(0, W_L1_OFFSET1);
                            flex_redmule_set_Y(0, YZ_L1_OFFSET);
                            flex_redmule_set_Z(0, YZ_L1_OFFSET);
                            flex_redmule_trigger_async(0);                                                  //Run RedMule Acceleration
                            flex_redmule_trigger_wait(0);                                                   //Wait RedMule Done
                        }
                    } else {
                        if (flex_is_dm_core()){                                                             //Use DM core to trigger DMA transcations
                            flex_dma_async_1d(local(X_L1_OFFSET1),hbm_addr(X_hbm_addr), TILE_SIZE_BYTE);    //Trigger DMA transaction: move X tile from HBM to L1
                            flex_dma_async_1d(local(W_L1_OFFSET1),hbm_addr(W_hbm_addr), TILE_SIZE_BYTE);    //Trigger DMA transaction: move W tile from HBM to L1
                            flex_dma_async_wait_all();                                                      //Wait all DMA transaction done
                        }

                        if (flex_is_first_core())                                                           //Use the first core in cluster 0 to configure and trigger RedMule
                        {
                            flex_redmule_set_X(0, X_L1_OFFSET2);                                            //Configure tile address in L1
                            flex_redmule_set_W(0, W_L1_OFFSET2);
                            flex_redmule_set_Y(0, YZ_L1_OFFSET);
                            flex_redmule_set_Z(0, YZ_L1_OFFSET);
                            flex_redmule_trigger_async(0);                                                  //Run RedMule Acceleration
                            flex_redmule_trigger_wait(0);                                                   //Wait RedMule Done
                        }
                    }
                    flex_intra_cluster_sync();                                                              //Cluster barrier
                }

                //Last Computation
                if (flex_is_first_core())
                {
                    flex_redmule_set_X(0, X_L1_OFFSET2);
                    flex_redmule_set_W(0, W_L1_OFFSET2);
                    flex_redmule_set_Y(0, YZ_L1_OFFSET);
                    flex_redmule_set_Z(0, YZ_L1_OFFSET);
                    flex_redmule_trigger_async(0);
                    flex_redmule_trigger_wait(0);
                }
                flex_intra_cluster_sync();                                                                  //Cluster barrier

                //Store Z tile
                if (flex_is_dm_core()){
                    flex_dma_async_1d(hbm_addr(Z_hbm_addr),local(YZ_L1_OFFSET), TILE_SIZE_BYTE);            //Trigger DMA transaction: move Z tile from L1 to HBM
                    flex_dma_async_wait_all();                                                              //Wait all DMA transaction done     
                }
                flex_intra_cluster_sync();                                                                  //Cluster barrier
            }
        }
    }

    flex_global_barrier_xy();                                                                               //Global barrier

}


#endif