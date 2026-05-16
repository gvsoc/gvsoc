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

uint16_t * L1_A1_addess __attribute__((section(".l1_share")));
uint16_t * L1_B1_addess __attribute__((section(".l1_share")));
uint16_t * L1_A2_addess __attribute__((section(".l1_share")));
uint16_t * L1_B2_addess __attribute__((section(".l1_share")));
uint16_t * L1_C_addess __attribute__((section(".l1_share")));

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
    uint32_t cluster_id_x = cluster_id % 4;
    uint32_t cluster_id_y = cluster_id / 4;

    //Allocate L1 memory for A, B, C tiles
    if(core_id == 0) { // Only core 0 in cluster 0 will perform memory allocation
        L1_A1_addess = (uint16_t *)flex_l1_malloc(A_TILE_SIZE_BYTES);
        L1_B1_addess = (uint16_t *)flex_l1_malloc(B_TILE_SIZE_BYTES);
        L1_A2_addess = (uint16_t *)flex_l1_malloc(A_TILE_SIZE_BYTES);
        L1_B2_addess = (uint16_t *)flex_l1_malloc(B_TILE_SIZE_BYTES);
        L1_C_addess = (uint16_t *)flex_l1_malloc(C_TILE_SIZE_BYTES);
    }

    // Synchronize to ensure memory allocation is done before other core accesses the L1 addresses
    flex_intra_cluster_sync();
    uint32_t m = cluster_id_y * TILE_M;
    uint32_t k = cluster_id_x * TILE_K;


    /******************/
    /* Your Code Here */
    /******************/



    // Check the result after DMA transfer
    flex_global_barrier();
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