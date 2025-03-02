## HBM Accesses 🚀

This tutorial walks you through basic operations with HBM (High-Bandwidth Memory) in **SoftHier**:
1. Preloading data into HBM before simulation
2. Moving data between HBM and cluster L1 memory
3. Dumping HBM data out to a file

---
## 1. Preload Data into HBM 📦

**Scenario:**  
You have four matrices \( A, B, C, D \) of dimension \( 64 \times 64 \) in **float16** format. You want to **preload** these matrices contiguously into SoftHier HBM at specific addresses.

### 1.1 Generate Matrices in Python 🐍
```python
import numpy as np

rng = np.random.default_rng()
A = rng.random((64, 64)).astype(np.float16)
B = rng.random((64, 64)).astype(np.float16)
C = rng.random((64, 64)).astype(np.float16)
D = rng.random((64, 64)).astype(np.float16)
```

### 1.2 Create a Preload ELF File 📂

SoftHier provides a utility (source code in `soft_hier/flex_cluster_utilities/preload.py`) to convert NumPy arrays into a preload ELF file. The key function is:

```python
def make_preload_elf(output_file_path, np_arrays, start_addresses=None):
    """
    Generate an ELF file preloading numpy arrays.

    Parameters:
    - output_file_path (str): Path to save the output ELF file.
    - np_arrays (list of numpy.ndarray): List of numpy arrays to include in the ELF.
    - start_addresses (list of int or None): List of starting addresses for each array,
      or None. If None, addresses are auto-determined with 64-byte alignment.
    """
    # Implementation...
```

You can specify:
- The **output path** of the generated preload ELF
- A **list of NumPy arrays** to be loaded
- A **list of base addresses** for each NumPy array

```python
import os
import numpy as np
import preload as pld

rng = np.random.default_rng()
A = rng.random((64, 64)).astype(np.float16)
B = rng.random((64, 64)).astype(np.float16)
C = rng.random((64, 64)).astype(np.float16)
D = rng.random((64, 64)).astype(np.float16)

matrix_size_in_byte = A.size * 2  # float16 is 2 bytes each
HBM_base_address     = 0xc0000000

preload_A_into_HBM_addr = HBM_base_address + matrix_size_in_byte * 0
preload_B_into_HBM_addr = HBM_base_address + matrix_size_in_byte * 1
preload_C_into_HBM_addr = HBM_base_address + matrix_size_in_byte * 2
preload_D_into_HBM_addr = HBM_base_address + matrix_size_in_byte * 3

pld.make_preload_elf(
    "my_preload.elf",
    [A, B, C, D],
    [
      preload_A_into_HBM_addr,
      preload_B_into_HBM_addr,
      preload_C_into_HBM_addr,
      preload_D_into_HBM_addr
    ]
)
```

Running the script above generates `my_preload.elf`. In the tutorial folder, you can try:

```bash
make hbm_pld
```
This command automatically creates the ELF file (`<PWD>/preload/my_preload.elf`) for you.

---
## 2. Load HBM Data into Cluster L1 🌐

**SoftHier** uses **iDMA** to move data between HBM and cluster L1 memory. In `soft_hier/flex_cluster_sdk/runtime/include/flex_dma_pattern.h`, the most commonly used APIs are:

```c
// Basic DMA 1D transfer (asynchronous)
void flex_dma_async_1d(uint64_t dst_addr, uint64_t src_addr, size_t transfer_size) {
    bare_dma_start_1d(dst_addr, src_addr, transfer_size); // Start iDMA
}

// Wait for all iDMA transactions to complete
void flex_dma_async_wait_all() {
    bare_dma_wait_all(); // Wait for iDMA finishing
}
```

> **Note:** Each cluster’s last core is connected to the iDMA. Use `if (flex_is_dm_core())` to ensure only the DMA core initiates DMA transfers.

Below is a **C** example showing how cluster 0 loads matrix **A** from HBM into local L1:

```c
#include "flex_runtime.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();

    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    // Example addresses
    uint64_t A_matrix_in_HBM_offset = 0;
    uint32_t A_matrix_in_L1_offset  = 0;
    uint32_t transfer_size          = 64 * 64 * 2;  // float16 is 2 bytes

    if (flex_is_dm_core() && (flex_get_cluster_id() == 0))
    {
        volatile uint16_t * local_ptr = (volatile uint16_t *)local(A_matrix_in_L1_offset);

        printf("[Before load HBM to L1] The first 8 elements of local L1 buffer are:\n");
        for (int i = 0; i < 8; ++i) {
            printf("    0x%04x\n", local_ptr[i]);
        }

        flex_dma_async_1d(local(A_matrix_in_L1_offset),
                          hbm_addr(A_matrix_in_HBM_offset),
                          transfer_size);
        printf("[Now    load HBM to L1] Loading with asynchronous API\n");
        flex_dma_async_wait_all();

        printf("[After  load HBM to L1] The first 8 elements of local L1 buffer are:\n");
        for (int i = 0; i < 8; ++i) {
            printf("    0x%04x\n", local_ptr[i]);
        }
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}
```

To run this with preload data in HBM, specify your preload ELF:
```bash
cfg=$(abspath config/arch.py) \
app=$(abspath application) \
pld=$(abspath preload/my_preload.elf) \
make -C ../../ hs run
```
Or simply use:
```bash
make hbm_l1
```

---
## 3. Store Data Back to HBM 💾

Storing data back to HBM follows the same pattern, but **destination** is HBM and **source** is local L1. For instance:

```c
#include "flex_runtime.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"


int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    uint64_t A_matrix_in_HBM_offset = 0;
    uint32_t A_matrix_in_L1_offset = 0;
    uint32_t transfer_size = 64 * 64 * 2;
    if (flex_is_dm_core() && (flex_get_cluster_id() == 0))
    {
        volatile uint16_t * local_ptr = (volatile uint16_t *)local(A_matrix_in_L1_offset);
        printf("[Before load HBM to L1] the first 8 elements of local L1 buffer are:\n");
        for (int i = 0; i < 8; ++i)
        {
            printf("    0x%04x\n", local_ptr[i]);
        }

        flex_dma_async_1d(local(A_matrix_in_L1_offset), hbm_addr(A_matrix_in_HBM_offset), transfer_size);
        printf("[Now    load HBM to L1] loading with asynchronize API\n");
        flex_dma_async_wait_all();

        printf("[After  load HBM to L1] the first 8 elements of local L1 buffer are:\n");
        for (int i = 0; i < 8; ++i)
        {
            printf("    0x%04x\n", local_ptr[i]);
        }

        uint64_t destination_in_HBM_offest = 4 * transfer_size;
        flex_dma_async_1d(hbm_addr(destination_in_HBM_offest), local(A_matrix_in_L1_offset), transfer_size);
        printf("[Now    store L1 to HBM] storing with asynchronize API\n");
        flex_dma_async_wait_all();
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}
```

Try:
```bash
make l1_hbm
```
to see the store-back in action.

---
## 4. Dump HBM Data to a File 📤

When the simulation ends, you can **dump HBM data** to a file. The APIs are in `soft_hier/flex_cluster_sdk/runtime/include/flex_dump.h`:

```c
void flex_dump_open(){
    volatile uint32_t * dump_reg = (volatile uint32_t *)(ARCH_CLUSTER_REG_BASE + ARCH_CLUSTER_REG_SIZE);
    *dump_reg = flex_get_enable_value();
}

void flex_dump_close(){
    volatile uint32_t * dump_reg = (volatile uint32_t *)(ARCH_CLUSTER_REG_BASE + ARCH_CLUSTER_REG_SIZE + 8);
    *dump_reg = flex_get_enable_value();
}

void flex_dump_hbm(uint64_t hbm_offset, size_t size){
    flex_dump_set_base(hbm_offset);
    flex_dma_async_1d((ARCH_CLUSTER_TCDM_BASE + ARCH_CLUSTER_TCDM_SIZE),
                      hbm_addr(hbm_offset),
                      size);
    flex_dma_async_wait_all();
}
```

> **Note:** Dumping data also uses **iDMA**, so ensure you’re on the **DMA core** (`if (flex_is_dm_core())`).

Typical usage:
1. `flex_dump_open()` to create a dump file named `dump_<cluster_id>`
2. Use `flex_dump_hbm()` to dump data from HBM to file
3. `flex_dump_close()` to finish dumping

Below is a full example:

```c
#include "flex_runtime.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"
#include "flex_dump.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();

    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    uint64_t A_matrix_in_HBM_offset = 0;
    uint32_t A_matrix_in_L1_offset  = 0;
    uint32_t transfer_size          = 64 * 64 * 2;

    if (flex_is_dm_core() && (flex_get_cluster_id() == 0))
    {
        volatile uint16_t * local_ptr = (volatile uint16_t *)local(A_matrix_in_L1_offset);
        printf("[Before load HBM to L1] The first 8 elements:\n");
        for (int i = 0; i < 8; ++i) {
            printf("    0x%04x\n", local_ptr[i]);
        }

        // Load
        flex_dma_async_1d(local(A_matrix_in_L1_offset),
                          hbm_addr(A_matrix_in_HBM_offset),
                          transfer_size);
        printf("[Now    load HBM to L1] Loading asynchronously\n");
        flex_dma_async_wait_all();

        printf("[After  load HBM to L1] The first 8 elements:\n");
        for (int i = 0; i < 8; ++i) {
            printf("    0x%04x\n", local_ptr[i]);
        }

        // Store back to HBM at some offset
        uint64_t destination_in_HBM_offset = 4 * transfer_size;
        flex_dma_async_1d(hbm_addr(destination_in_HBM_offset),
                          local(A_matrix_in_L1_offset),
                          transfer_size);
        printf("[Now    store L1 to HBM] Storing asynchronously\n");
        flex_dma_async_wait_all();

        // Dump the original and stored data
        printf("[Dump  HBM data to File]\n");
        flex_dump_open();
        flex_dump_hbm(A_matrix_in_HBM_offset, transfer_size);
        flex_dump_hbm(destination_in_HBM_offset, transfer_size);
        flex_dump_close();
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}
```

After simulation, you’ll find a file named **`dump_0`** in the SoftHier repository root. It contains the hex data from HBM:

```
HBM offset == .0x0000000000000000:
  0x3674
  0x2ec9
  0x2d8d
  0x2f3e
  ...

HBM offset == .0x0000000000008000:
  0x3674
  0x2ec9
  0x2d8d
  0x2f3e
  ...
```

---
## Summary 🏁

- **Preload** data with `make_preload_elf`
- **Transfer** data via iDMA using:
  - `flex_dma_async_1d(destination, source, size)`
  - `flex_dma_async_wait_all()`
- **Dump** your HBM data to a file with `flex_dump_*` APIs

You’re now ready to manage HBM data in **SoftHier** for your ML workloads or any other high-performance applications. Happy coding! 🧩✨