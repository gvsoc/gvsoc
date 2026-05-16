#include "flex_runtime.h"
#include "flex_alloc.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"
#include "util.h"

#define MATRIX_DIM 4
#define MATRIX_SIZE_BYTES (MATRIX_DIM * MATRIX_DIM * sizeof(uint16_t))

uint16_t A_in_HBM[MATRIX_DIM * MATRIX_DIM] __attribute__((section(".hbm_west"))) =
    {1,  2,  3,  4,
     5,  6,  7,  8,
     9,  10, 11, 12,
     13, 14, 15, 16};

uint16_t * L1_buffer_addess __attribute__((section(".l1_share")));

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_init();
    flex_alloc_init();
    flex_global_barrier();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    uint32_t cluster_id = flex_get_cluster_id(); // Get cluster ID
    uint32_t core_id = flex_get_core_id(); // Get core ID
    if(cluster_id == 0) { // Only cluster 0 will perform memory allocation and data movement
        if(core_id == 1) { // Only core 1 (with DMA attached) will perform memory allocation to the L1 buffer

            // Allocate L1 buffer for A
            L1_buffer_addess = (uint16_t *)flex_l1_malloc(MATRIX_SIZE_BYTES);

            // DMA transfer from HBM to L1
            flex_dma_async_1d(
                (uint32_t)L1_buffer_addess, // destination address in L1
                (uint32_t)A_in_HBM, // source address in HBM
                MATRIX_SIZE_BYTES); // size of data to transfer

            // Wait for DMA transfer to complete
            flex_dma_async_wait_all();
        }
    }

    // Check the result after DMA transfer
    flex_global_barrier();
    if(cluster_id == 0 && core_id == 0) { // Only core 0 in cluster 0 will print the result
        printf("Matrix A in L1:\n");
        print_array_uint16(L1_buffer_addess, MATRIX_DIM, MATRIX_DIM);
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier();
    flex_eoc(eoc_val);
    return 0;
}