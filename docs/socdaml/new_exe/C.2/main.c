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
    performance_counter_start();
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

    flex_intra_cluster_sync();
    uint32_t m                  = cluster_id_y * TILE_M;
    uint32_t k                  = cluster_id_x * TILE_K;
    uint32_t start_iter         = cluster_id_x + cluster_id_y; // Interesting! think about it, why we need this?
    uint32_t stop_iter          = start_iter + N/TILE_N;
    uint32_t west_cluster_id    = cluster_id_y * 4 + (cluster_id_x - 1);
    uint32_t south_cluster_id   = (cluster_id_y - 1) * 4 + cluster_id_x;
    uint32_t is_on_west_edge    = (cluster_id_x == 0);
    uint32_t is_on_south_dege   = (cluster_id_y == 0);

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
    flex_global_barrier();

    
    for (int i = 0; i < (N/TILE_N + 7); ++i)
    {
        int iter = i - start_iter;
        uint32_t n = iter * TILE_N;

        // DMA Pattern
        if (core_id == 1 && i >= start_iter && i < stop_iter)
        {
            if (is_on_west_edge)
            {
                flex_dma_async_2d(
                    (iter%2 == 0)? (uint32_t)L1_A1_addess : (uint32_t)L1_A2_addess, // destination address in L1
                    (uint32_t)A_in_HBM + (m * N + n) * sizeof(uint16_t), // source address in HBM
                    TILE_N * sizeof(uint16_t), // size of each tiled row
                    TILE_N * sizeof(uint16_t), // dest stride (row stride in L1)
                    N * sizeof(uint16_t), // source stride (row stride in HBM)
                    TILE_M);// number of rows
            } else {
                flex_dma_async_1d(
                    (iter%2 == 0)? (uint32_t)L1_A1_addess : (uint32_t)L1_A2_addess, // destination address in L1
                    (iter%2 == 0)? remote_cid(west_cluster_id,(uint32_t)L1_A1_addess) : remote_cid(west_cluster_id,(uint32_t)L1_A2_addess), // source address in remote cluster L1
                    A_TILE_SIZE_BYTES); // size of data to transfer
            }

            if (is_on_south_dege)
            {
                flex_dma_async_2d(
                    (iter%2 == 0)? (uint32_t)L1_B1_addess : (uint32_t)L1_B2_addess, // destination address in L1
                    (uint32_t)B_in_HBM + (n * K + k) * sizeof(uint16_t), // source address in HBM
                    TILE_K * sizeof(uint16_t), // size of each tiled row
                    TILE_K * sizeof(uint16_t), // dest stride (row stride in L1)
                    K * sizeof(uint16_t), // source stride (row stride in HBM)
                    TILE_N);// number of rows
            } else {
                flex_dma_async_1d(
                    (iter%2 == 0)? (uint32_t)L1_B1_addess : (uint32_t)L1_B2_addess, // destination address in L1
                    (iter%2 == 0)? remote_cid(south_cluster_id,(uint32_t)L1_B1_addess) : remote_cid(south_cluster_id,(uint32_t)L1_B2_addess), // source address in remote cluster L1
                    B_TILE_SIZE_BYTES); // size of data to transfer
            }

            // Wait for DMA transfer to complete
            flex_dma_async_wait_all();
        }

        //RedMule Pattern
        if (core_id == 0 && i >= (start_iter + 1) && i < (stop_iter + 1))
        {
            if (i > start_iter + 1) flex_redmule_wait();

            // Configure RedMule for matrix multiplication
            flex_redmule_config(TILE_M, TILE_N, TILE_K);

            // Trigger RedMule for matrix multiplication with FP16 format
            flex_redmule_trigger(
                (iter%2 == 1)? (uint32_t)L1_A1_addess : (uint32_t)L1_A2_addess,
                (iter%2 == 1)? (uint32_t)L1_B1_addess : (uint32_t)L1_B2_addess,
                (uint32_t)L1_C_addess,
                REDMULE_UINT_16);

            if (i == stop_iter) flex_redmule_wait();
        }

        flex_global_barrier();
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