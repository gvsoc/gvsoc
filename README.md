# SoftHier Simulation Model in GVSoC

## SoftHier Architecture Overview

![SoftHier Architecture Diagram](docs/figures/SoftHier_Arch.png)

## OS Requirements Installation

The following instructions are designed for a fresh installation of Ubuntu 22.04 (Jammy Jellyfish).

To install the required packages, run:

```bash
sudo apt-get install -y build-essential git doxygen python3-pip libsdl2-dev curl cmake gtkwave libsndfile1-dev rsync autoconf automake texinfo libtool pkg-config libsdl2-ttf-dev
```

## Toolchain Requirements

GVSoC requires the following tools and versions:

- **g++** and **gcc** versions >= 11.2.0
- **cmake** version >= 3.18.1
- **Python** version >= 3.10

Please ensure your toolchain meets these requirements. You can set the following environment variables as an example (adjust the versions as per your setup):

```bash
export CXX=g++-11.2.0
export CC=gcc-11.2.0
export CMAKE=cmake-3.18.1
```

## Getting Started with SoftHier Simulation

To begin a SoftHier simulation, follow these steps:

1. Clone the repository and navigate into the project directory:

   ```bash
   git clone https://github.com/gvsoc/gvsoc.git -b soft_hier_flex_cluster soft_hier
   cd soft_hier
   ```

2. Export the necessary toolchain environment variables (adjust the versions as per your setup):

   ```bash
   export CXX=g++-11.2.0
   export CC=gcc-11.2.0
   export CMAKE=cmake-3.18.1
   ```

3. Set up the simulator by running the following command:

   ```bash
   source softhier_pushbutton.sh
   ```

4. Build the SoftHier model and the execution binary:

   ```bash
   make build_softhier
   ```

5. Run the simulation:

   ```bash
   make run
   ```

## SoftHier Simulation Tutorial

All SoftHier model source code can be found in the `add_dramsyslib_patches/flex_cluster/` directory. The software stack is located in the `add_dramsyslib_patches/flex_cluster_sdk/test` directory.

### SoftHier Architecture Configuration

The architecture configuration is managed through the Python file `add_dramsyslib_patches/flex_cluster/flex_cluster_arch.py`, which includes parameters such as:

- **Cluster Configuration**:
  - Number of clusters in the mesh (X and Y dimensions)
  - Number of Snitch (RISC-V) cores per cluster
  - TCDM (L1) banks: width and number

- **RedMule Configuration**:
  - Number of RedMule cores per cluster
  - CE array size (height and width)
  - CE pipeline stages
  - Element size (in bytes) for operations
  - RedMule access queue depth

- **DMA Configuration**:
  - Number of outstanding transactions
  - Burst length per transaction

- **HBM Configuration**:
  - Number of HBM channels per cluster (at edge)
  - HBM channel placement (West, North, East, South edges)

- **NoC Configuration**:
  - Number of outstanding transactions per router
  - NoC link data width

The file also contains detailed system address mappings.

Here is an example of the configuration:

```python
class FlexClusterArch:

    def __init__(self):

        #Cluster
        self.num_cluster_x           = 4
        self.num_cluster_y           = 4
        self.num_core_per_cluster    = 3

        self.cluster_tcdm_bank_width = 8
        self.cluster_tcdm_bank_nb    = 64

        self.cluster_tcdm_base       = 0x00000000
        self.cluster_tcdm_size       = 0x00100000
        self.cluster_tcdm_remote     = 0x30000000

        self.cluster_stack_base      = 0x10000000
        self.cluster_stack_size      = 0x00020000

        self.cluster_reg_base        = 0x20000000
        self.cluster_reg_size        = 0x00000200

        #RedMule
        self.num_redmule_per_cluster = 1
        self.redmule_ce_height       = 128
        self.redmule_ce_width        = 32
        self.redmule_ce_pipe         = 3
        self.redmule_elem_size       = 2
        self.redmule_queue_depth     = 1
        self.redmule_reg_base        = 0x20020000
        self.redmule_reg_size        = 0x00000200

        #IDMA
        self.idma_outstand_txn       = 16
        self.idma_outstand_burst     = 256

        #HBM
        self.hbm_start_base          = 0xc0000000
        self.hbm_node_interleave     = 0x00100000
        self.num_hbm_ch_per_node     = 1
        self.hbm_placement           = [4,0,0,4]

        #NoC
        self.noc_outstanding         = 64
        self.noc_link_width          = 512

        #System
        self.instruction_mem_base    = 0x80000000
        self.instruction_mem_size    = 0x00010000

        self.soc_register_base       = 0x90000000
        self.soc_register_size       = 0x00010000
        self.soc_register_eoc        = 0x90000000
        self.soc_register_wakeup     = 0x90000004

        #Synchronization
        self.sync_base               = 0x40000000
        self.sync_interleave         = 0x00000080
        self.sync_special_mem        = 0x00000040

```

### SoftHier Software

The entry point for programs on the SoftHier architecture is located in the file `add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c`. By default, it runs an example of GEMM (General Matrix Multiply) using one cluster.

```C
#include "flex_runtime.h"
#include "kernels/gemm/gemm_systolic_wise.h"
#include "examples/example_one_cluster_gemm.h"
#include <math.h>

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    flex_timer_start();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    example_one_cluster_gemm();

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_timer_end();
    flex_eoc(eoc_val);
	return 0;
}
```
The function of example_one_cluster_gemm is defined in file `add_dramsyslib_patches/flex_cluster_sdk/test/include/examples/example_one_cluster_gemm.h`. The code demonstrates how to:

1. Use DMA to transfer data between HBM and the cluster.
2. Use RedMule to accelerate GEMM operations.

```C
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
```
