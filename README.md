# SoftHier Simulation Model in GVSoC 🚀

## SoftHier Architecture Overview 🏗️

![SoftHier Architecture Diagram](docs/figures/SoftHier_Arch.png)

## OS Requirements Installation 🖥️

The following instructions are designed for a fresh installation of Ubuntu 22.04 (Jammy Jellyfish).

To install the required packages, run:

```bash
sudo apt-get install -y build-essential git doxygen python3-pip libsdl2-dev curl cmake gtkwave libsndfile1-dev rsync autoconf automake texinfo libtool pkg-config libsdl2-ttf-dev
```

## Toolchain and Shell Requirements 🔧

GVSoC requires the following tools and versions:

- **g++** and **gcc** versions >= 11.2.0
- **cmake** version >= 3.18.1
- **Python** version >= 3.11.3

Please ensure your toolchain meets these requirements. 

Also please make sure you are using the bash shell for SoftHier Simulation:

```bash
bash
```

## Getting Started with SoftHier Simulation 🚀

### Clone the Repository and Set Up the Environment 🏁

Follow these steps to set up the SoftHier simulation environment:

1. **Clone the repository** and navigate into the project directory:

   ```bash
   git clone https://github.com/gvsoc/gvsoc.git -b soft_hier_release soft_hier_release
   cd soft_hier_release
   ```

2. **Initialize the simulator environment** by running:

   ```bash
   source sourceme.sh
   ```

### Build and Run the SoftHier Simulation Model 🧱

1. **Build the SoftHier hardware model**: 🛠️

   ```bash
   make hw
   ```
The default configuration file is located at `soft_hier/flex_cluster/flex_cluster_arch.py`. To use a custom architecture configuration, specify the file path as follows:

   ```bash
   cfg=<path/to/your/architecture/configuration/file> make hw
   ```

2. **Run the simulation** with an example binary: 🎮

   ```bash
   ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary examples/SoftHier/binary/example.elf run --trace=/chip/cluster_0/redmule
   ```

   - `--binary`: Specifies the executable binary to be loaded for the SoftHier simulation.
   - `--trace`: Indicates which component's trace logs should be generated during the simulation.


### Build the Default Binary 💾
To build the default binary from the source code in `soft_hier/flex_cluster_sdk/app_example`, run:
   ```bash
   make sw
   ```
The generated binary `sw_build/softhier.elf` and the dump file `sw_build/softhier.dump` will be located in the `sw_build` directory.

### Build a Custom Binary ✏️
To build your own binary:

1. Prepare your source code in a folder with a `CMakeLists.txt` that defines the source files and include paths. For example:
   ```cmake
   # CMakeLists.txt example
   set(SRC_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/main.c
   )
   
   set(SOURCES ${SRC_SOURCES} PARENT_SCOPE)
   set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
   ```

2. Run the following command, replacing `<folder/of/your/code>` with the path to your source code folder:
   ```bash
   app=<folder/of/your/code> make sw
   ```

This will compile the binary using the specified folder. The generated binary `sw_build/softhier.elf` and the dump file `sw_build/softhier.dump` will be located in the `sw_build` directory.


### Build Customized Hardware and Software (Highly Recommended) 🧩

For convenient and flexible development, use the `hs` Makefile target to build both hardware and software together. This is particularly useful for custom architecture configurations and software development. Run:

```bash
cfg=<path/to/your/architecture/configuration/file> app=<folder/of/your/code> make hs
```

We provide example architecture configurations and software source code in the repository. Try the following:

#### Example 1: Hello World 🌍
```bash
cfg=examples/SoftHier/config/arch_test.py app=examples/SoftHier/software/test make hs; make run
```

#### Example 2:  Dataflow 🎯
1. **Systolic GEMM on 512-bit NoC Bus**
   ```bash
   cfg=examples/SoftHier/config/arch_NoC512.py app=examples/SoftHier/software/gemm_systolic make hs; make runv
   ```
2. **Systolic GEMM on 1024-bit NoC Bus**
   ```bash
   cfg=examples/SoftHier/config/arch_NoC1024.py app=examples/SoftHier/software/gemm_systolic make hs; make runv
   ```

#### Example 3:  GEMM with HBM Preloading 🚀
The `examples/SoftHier/assembled/HBM_preload_example/` directory contains:
- **`config/`** – Architecture configuration files.
- **`preload/`** – Preloaded data files.
- **`software/`** – Multi-cluster GEMM software.

1. **Generating HBM Preload Data**
To generate an HBM preload binary from NumPy data, run:
```bash
python examples/SoftHier/assembled/HBM_preload_example/preload/hbm_data.py
```
This will create the preload binary at:
```
examples/SoftHier/assembled/HBM_preload_example/preload/preload.elf
```

2. **Running SoftHier with HBM Preload**
To run SoftHier with the preloaded HBM data, use:
```bash
cfg=examples/SoftHier/assembled/HBM_preload_example/config/arch.py \
app=examples/SoftHier/assembled/HBM_preload_example/software \
pld=examples/SoftHier/assembled/HBM_preload_example/preload/preload.elf \
make hs runv
```

## SoftHier Visualization 📈

To visualize a SoftHier simulation, follow these steps:
1. Run with the `runv` Makefile target.
2. Generate a Perfetto-format trace file using the `pfto` target.
An integrated example
```bash
<args> make hs runv pfto
```

The trace file will be saved at:  
📂 `sw_build/perfetto.json`

To view the trace, open the following URL in your browser:  
👉 [Perfetto UI](https://ui.perfetto.dev/)

## SoftHier Simulation Tutorial 📖

All SoftHier model source code can be found in the `soft_hier/flex_cluster/` directory. The software stack is located in the `soft_hier/flex_cluster_sdk/runtime` directory.

### SoftHier Architecture Configuration ⚙️

The architecture configuration is managed through the Python file `soft_hier/flex_cluster/flex_cluster_arch.py`, which includes parameters such as:

- **Cluster Configuration**:
  - Number of clusters in the mesh (X and Y dimensions)
  - Number of Snitch (RISC-V) cores per cluster
  - TCDM (L1) banks: width and number

- **RedMule Configuration**:
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

        self.cluster_tcdm_bank_width = 64
        self.cluster_tcdm_bank_nb    = 64

        self.cluster_tcdm_base       = 0x00000000
        self.cluster_tcdm_size       = 0x00100000
        self.cluster_tcdm_remote     = 0x30000000

        self.cluster_stack_base      = 0x10000000
        self.cluster_stack_size      = 0x00001000

        self.cluster_reg_base        = 0x20000000
        self.cluster_reg_size        = 0x00000200

        #RedMule
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
        self.hbm_placement           = [4,0,0,0]

        #NoC
        self.noc_outstanding         = 64
        self.noc_link_width          = 512
```

### SoftHier Software 💻

The entry point for programs on the SoftHier architecture is located in the file `soft_hier/flex_cluster_sdk/app_example/main.c`. By default, it runs an example of GEMM (General Matrix Multiply) using one cluster.

```C
#include "flex_runtime.h"
#include "example_one_cluster_gemm.h"
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
The function of example_one_cluster_gemm is defined in file `soft_hier/flex_cluster_sdk/app_example/include/example_one_cluster_gemm.h`. The code demonstrates how to:

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
// Elem Size            : FP16
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
    flex_global_barrier_xy();//Global barrier

    uint32_t CID = flex_get_cluster_id();//Get cluster ID

    //Initialize RedMule Paramters
    if (flex_is_first_core() && (CID == 0))//Use the first core in cluster 0 to configure RedMule
    {
        //Configure M-N-K of tile that RedMule will accelerate
        flex_redmule_config(TILE_DIMENSION, TILE_DIMENSION, TILE_DIMENSION);
    }

    flex_global_barrier_xy();//Global barrier

    if (CID == 0)//Only let Cluster 0 to work
    {
        //Iterate over every Z tiles
        for (int row = 0; row < TILES_PER_DIM; ++row)
        {
            for (int col = 0; col < TILES_PER_DIM; ++col)
            {
                //Start address of tiles
                uint32_t X_hbm_addr = X_HBM_OFFSET + row * TILES_PER_DIM * TILE_SIZE_BYTE;
                uint32_t W_hbm_addr = W_HBM_OFFSET + col * TILE_SIZE_BYTE;
                uint32_t Y_hbm_addr = Y_HBM_OFFSET + row * TILES_PER_DIM * TILE_SIZE_BYTE + col * TILE_SIZE_BYTE;
                uint32_t Z_hbm_addr = Z_HBM_OFFSET + row * TILES_PER_DIM * TILE_SIZE_BYTE + col * TILE_SIZE_BYTE;


                //Preload Y,X,W tiles
                if (flex_is_dm_core()){//Use DM core to trigger DMA transcations
                    //Trigger DMA transaction: move Y tile from HBM to L1
                    flex_dma_async_1d(local(YZ_L1_OFFSET),hbm_addr(Y_hbm_addr), TILE_SIZE_BYTE);

                    //Trigger DMA transaction: move X tile from HBM to L1
                    flex_dma_async_1d(local(X_L1_OFFSET1),hbm_addr(X_hbm_addr), TILE_SIZE_BYTE);

                    //Trigger DMA transaction: move W tile from HBM to L1
                    flex_dma_async_1d(local(W_L1_OFFSET1),hbm_addr(W_hbm_addr), TILE_SIZE_BYTE);

                    //Wait all DMA transaction done
                    flex_dma_async_wait_all();
                }
                flex_intra_cluster_sync();//Cluster barrier

                //Compute X and W to YZ in L1 + preload next X and W
                for (int i = 0; i < (TILES_PER_DIM-1); ++i)
                {
                    //Calculate next X and W tile address in HBM
                    X_hbm_addr += TILE_SIZE_BYTE;
                    W_hbm_addr += TILES_PER_DIM * TILE_SIZE_BYTE;

                    //Ping-Pong operations on Double-Buffering X and W
                    if (i%2 == 0)
                    {
                        if (flex_is_dm_core()){//Use DM core to trigger DMA transcations
                            //Trigger DMA transaction: move X tile from HBM to L1
                            flex_dma_async_1d(local(X_L1_OFFSET2),hbm_addr(X_hbm_addr), TILE_SIZE_BYTE);

                            //Trigger DMA transaction: move W tile from HBM to L1
                            flex_dma_async_1d(local(W_L1_OFFSET2),hbm_addr(W_hbm_addr), TILE_SIZE_BYTE);

                            //Wait all DMA transaction done
                            flex_dma_async_wait_all();
                        }

                        if (flex_is_first_core())//Use the first core in cluster 0 to configure and trigger RedMule
                        {
                            //Configure tile address in L1 and run RedMule acceleration
                            flex_redmule_trigger(X_L1_OFFSET1, W_L1_OFFSET1, YZ_L1_OFFSET, REDMULE_FP_16);

                            //Wait RedMule Done
                            flex_redmule_wait();
                        }
                    } else {
                        if (flex_is_dm_core()){//Use DM core to trigger DMA transcations
                            //Trigger DMA transaction: move X tile from HBM to L1
                            flex_dma_async_1d(local(X_L1_OFFSET1),hbm_addr(X_hbm_addr), TILE_SIZE_BYTE);

                            //Trigger DMA transaction: move W tile from HBM to L1
                            flex_dma_async_1d(local(W_L1_OFFSET1),hbm_addr(W_hbm_addr), TILE_SIZE_BYTE);

                            //Wait all DMA transaction done
                            flex_dma_async_wait_all();
                        }

                        if (flex_is_first_core())//Use the first core in cluster 0 to configure and trigger RedMule
                        {
                            //Configure tile address in L1 and run RedMule acceleration
                            flex_redmule_trigger(X_L1_OFFSET2, W_L1_OFFSET2, YZ_L1_OFFSET, REDMULE_FP_16);

                            //Wait RedMule Done
                            flex_redmule_wait();
                        }
                    }
                    flex_intra_cluster_sync();//Cluster barrier
                }

                //Last Computation
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(X_L1_OFFSET2, W_L1_OFFSET2, YZ_L1_OFFSET, REDMULE_FP_16);
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();//Cluster barrier

                //Store Z tile
                if (flex_is_dm_core()){
                    //Trigger DMA transaction: move Z tile from L1 to HBM
                    flex_dma_async_1d(hbm_addr(Z_hbm_addr),local(YZ_L1_OFFSET), TILE_SIZE_BYTE);

                    //Wait all DMA transaction done
                    flex_dma_async_wait_all();
                }
                flex_intra_cluster_sync();//Cluster barrier
            }
        }
    }

    flex_global_barrier_xy();//Global barrier

}


#endif
```
