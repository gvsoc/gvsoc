#include "flex_runtime.h"
#include "flex_alloc.h"
#include "flex_dma_pattern.h"
#include "flex_printf.h"
#include "util.h"
#include "flex_redmule.h"

#define M 32
#define N 1024   // shared dimension
#define K 32

#define TILE_M 8
#define TILE_N 8
#define TILE_K 8

#define A_TILE_SIZE_BYTES (TILE_M * TILE_N * sizeof(uint16_t))
#define B_TILE_SIZE_BYTES (TILE_N * TILE_K * sizeof(uint16_t))
#define C_TILE_SIZE_BYTES (TILE_M * TILE_K * sizeof(uint16_t))

// A: [M x N] stored in West HBM
uint16_t A_in_HBM[M * N] __attribute__((section(".hbm_west"))) = {
    #include "A_in_HBM.txt"
};

// B: [N x K] stored in South HBM
uint16_t B_in_HBM[N * K] __attribute__((section(".hbm_south"))) = {
    #include "B_in_HBM.txt"
};

// C: [M x K] stored in South HBM (output)
uint16_t C_in_HBM[M * K] __attribute__((section(".hbm_south"))) = {
    #include "C_in_HBM.txt"
};


// Golden: [M x K] stored in West HBM (golden results to check)
uint16_t Golden_in_HBM[M * K] __attribute__((section(".hbm_west"))) = {
    #include "Golden_in_HBM.txt"
};

uint16_t * L1_A_addess __attribute__((section(".l1_share")));
uint16_t * L1_B_addess __attribute__((section(".l1_share")));
uint16_t * L1_C_addess __attribute__((section(".l1_share")));

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_init();
    flex_alloc_init();
    flex_global_barrier();
    performance_counter_start();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    uint32_t cluster_id = flex_get_cluster_id(); // Get cluster ID
    uint32_t core_id = flex_get_core_id(); // Get core ID
    if(cluster_id == 0) { // Only cluster 0 will perform memory allocation and data movement
        //Allocate L1 memory for A, B, C tiles
        if(core_id == 0) { // Only core 0 in cluster 0 will perform memory allocation
            L1_A_addess = (uint16_t *)flex_l1_malloc(A_TILE_SIZE_BYTES);
            L1_B_addess = (uint16_t *)flex_l1_malloc(B_TILE_SIZE_BYTES);
            L1_C_addess = (uint16_t *)flex_l1_malloc(C_TILE_SIZE_BYTES);
        }

        // Synchronize to ensure memory allocation is done before other core accesses the L1 addresses
        flex_intra_cluster_sync();

        //Iterate over tiles of C
        for(int m = 0; m < M; m += TILE_M) {
            for(int k = 0; k < K; k += TILE_K) {

                //load C tile from HBM to L1
                if(core_id == 1){
                    flex_dma_async_2d(
                        (uint32_t)L1_C_addess, // destination address in L1
                        (uint32_t)C_in_HBM + (m * K + k) * sizeof(uint16_t), // source address in HBM
                        TILE_K * sizeof(uint16_t), // size of each tiled row
                        TILE_K * sizeof(uint16_t), // dest stride (row stride in L1)
                        K * sizeof(uint16_t), // source stride (row stride in HBM)
                        TILE_M);// number of rows
                    
                    // Wait for DMA transfer to complete
                    flex_dma_async_wait_all();
                }
                flex_intra_cluster_sync();


                //Iterate over tiles of A and B for the current C tile, compute C += A*B in L1
                for(int n = 0; n < N; n += TILE_N) {
                    /*******************/
                    /* Your Code Here */
                    /*******************/

                }


                //store the computed C tile back to HBM
                if(core_id == 1){
                    flex_dma_async_2d(
                        (uint32_t)C_in_HBM + (m * K + k) * sizeof(uint16_t), // destination address in HBM
                        (uint32_t)L1_C_addess, // source address in L1
                        TILE_K * sizeof(uint16_t), // size of each row
                        K * sizeof(uint16_t), // dest stride (row stride in HBM)
                        TILE_K * sizeof(uint16_t), // source stride (row stride in L1)
                        TILE_M);// number of rows
                    
                    // Wait for DMA transfer to complete
                    flex_dma_async_wait_all();
                }
                flex_intra_cluster_sync();
            }
        }
    }

    // Check the result
    flex_global_barrier();
    performance_counter_stop();
    if(cluster_id == 0 && core_id == 0) { // Only core 0 in cluster 0 will print the result
        printf("Matrix C in HBM:\n");
        print_array_uint16(C_in_HBM, M, K);
        check_results_uint16(C_in_HBM, Golden_in_HBM, M, K);
    }

    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier();
    flex_eoc(eoc_val);
    return 0;
}