#include "flex_runtime.h"
#include "flex_alloc.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"
#include "util.h"
#include "flex_redmule.h"

#define MATRIX_DIM 4
#define MATRIX_SIZE_BYTES (MATRIX_DIM * MATRIX_DIM * sizeof(uint16_t))

uint16_t A_in_HBM[MATRIX_DIM * MATRIX_DIM] __attribute__((section(".hbm_west"))) =
    {1,  2,  3,  4,
     5,  6,  7,  8,
     9,  10, 11, 12,
     13, 14, 15, 16};


uint16_t B_in_HBM[MATRIX_DIM * MATRIX_DIM] __attribute__((section(".hbm_south"))) =
    {0, 0, 0, 1,
     0, 0, 1, 0,
     0, 1, 0, 0,
     1, 0, 0, 0};

uint16_t C_in_HBM[MATRIX_DIM * MATRIX_DIM] __attribute__((section(".hbm_south"))) =
    {0, 0, 0, 0,
     0, 0, 0, 0,
     0, 0, 0, 0,
     0, 0, 0, 0};

uint16_t Golden_in_HBM[MATRIX_DIM * MATRIX_DIM] __attribute__((section(".hbm_west"))) =
    {4,  3,  2,  1,
     8,  7,  6,  5,
     12, 11, 10, 9,
     16, 15, 14, 13};

uint16_t * L1_A_addess __attribute__((section(".l1_share")));
uint16_t * L1_B_addess __attribute__((section(".l1_share")));
uint16_t * L1_C_addess __attribute__((section(".l1_share")));

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_alloc_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    uint32_t cluster_id = flex_get_cluster_id(); // Get cluster ID
    uint32_t core_id = flex_get_core_id(); // Get core ID
    if(cluster_id == 0) { // Only cluster 0 will perform memory allocation and data movement
        if(core_id == 1) { // Only core 1 (with DMA attached) will perform memory allocation to the L1 buffer

            // Allocate L1 buffer for A
            L1_A_addess = (uint16_t *)flex_l1_malloc(MATRIX_SIZE_BYTES);
            // Allocate L1 buffer for B
            L1_B_addess = (uint16_t *)flex_l1_malloc(MATRIX_SIZE_BYTES);
            // Allocate L1 buffer for C
            L1_C_addess = (uint16_t *)flex_l1_malloc(MATRIX_SIZE_BYTES);

            // DMA transfer from HBM to L1
            flex_dma_async_1d(
                (uint32_t)L1_A_addess, // destination address in L1
                (uint32_t)A_in_HBM, // source address in HBM
                MATRIX_SIZE_BYTES); // size of data to transfer
            
            flex_dma_async_1d(
                (uint32_t)L1_B_addess, // destination address in L1
                (uint32_t)B_in_HBM, // source address in HBM
                MATRIX_SIZE_BYTES); // size of data to transfer
            
            flex_dma_async_1d(
                (uint32_t)L1_C_addess, // destination address in L1
                (uint32_t)C_in_HBM, // source address in HBM
                MATRIX_SIZE_BYTES); // size of data to transfer

            // Wait for DMA transfer to complete
            flex_dma_async_wait_all();
        }

        /*********************************/
        /* Should we add something here? */
        /*********************************/
        flex_intra_cluster_sync();

        if(core_id == 0) { // Only core 0 will configure and trigger the RedMule
            // Configure RedMule for matrix multiplication
            flex_redmule_config(MATRIX_DIM, MATRIX_DIM, MATRIX_DIM);

            // Trigger RedMule for matrix multiplication with FP16 format
            flex_redmule_trigger((uint32_t)L1_A_addess, (uint32_t)L1_B_addess, (uint32_t)L1_C_addess, REDMULE_UINT_16);

            // Wait for RedMule computation to complete
            flex_redmule_wait();
        }

        /*********************************/
        /* Should we add something here? */
        /*********************************/
        flex_intra_cluster_sync();

        if(core_id == 1) { // Only core 1 (with DMA attached) will perform memory allocation to the L1 buffer
            // DMA transfer from L1 to HBM
            flex_dma_async_1d(
                (uint32_t)C_in_HBM, // destination address in HBM
                (uint32_t)L1_C_addess, // source address in L1
                MATRIX_SIZE_BYTES); // size of data to transfer

            // Wait for DMA transfer to complete
            flex_dma_async_wait_all();
        }
    }

    // Check the result after DMA transfer
    flex_global_barrier_xy();
    if(cluster_id == 0 && core_id == 0) { // Only core 0 in cluster 0 will print the result
        printf("Matrix C in HBM:\n");
        print_array_uint16(C_in_HBM, MATRIX_DIM, MATRIX_DIM);
        check_results_uint16(C_in_HBM, Golden_in_HBM, MATRIX_DIM, MATRIX_DIM);
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}