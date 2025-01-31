// #include <cuda_runtime.h>
// #include <dace/dace.h>
#include <math.h>
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_printf.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

void show_progress_animated(int iter, int total) {
    int percentage = iter*100/total;
    int barWidth = 40;
    int progress = (percentage * barWidth) / 100;

    printf("\033[1;32m"); // Set color to green
    printf("\r[");
    
    for (int i = 0; i < barWidth; i++) {
        if (i < progress)
            printf("█");  // Block character for a solid look
        else
            printf(" ");
    }
    
    printf("] %d%% | %0d:%0d", percentage, total, iter);
    printf("\033[0m"); // Reset color
}

void gemm(const uint32_t A, const uint32_t B, const uint32_t C, const uint32_t K, const uint32_t M, const uint32_t N) {
    {
        flex_global_barrier_xy();
        uint32_t cluster_id = flex_get_cluster_id();
        uint32_t core_id = flex_get_core_id();
        {
            for (auto i = 0; i < M; i += 256) {
                for (auto j = 0; j < N; j += 256) {
                    {
                        int gi = get_pos(cluster_id).x;
                        int gj = get_pos(cluster_id).y;
                        if (gi <= 3) {
                            if (gj <= 3) {
                                // Minels: [0, 0], Maxels: [3, 3]
                                // Configure RedMule
                                if(flex_is_first_core())
                                {
                                    flex_redmule_config(64, 64, 64);
                                }
                                flex_intra_cluster_sync();
                                {
                                    for (auto ci = 0; ci < 64; ci += 64) {
                                        for (auto cj = 0; cj < 64; cj += 64) {
                                            uint32_t accumulator;
                                            accumulator = 0;
                                            if(flex_is_dm_core())
                                            {
                                                flex_dma_async_1d(local(accumulator), zomem(0), 8192);
                                                flex_dma_async_wait_all();
                                            }
                                            flex_intra_cluster_sync();

                                            {
                                                for (auto bK = 0; bK < K; bK += 64) {
                                                    uint32_t local_A;
                                                    local_A = 8192;
                                                    uint32_t local_B;
                                                    local_B = 16384;

                                                    // SoftHier_HBM -> SoftHier_TCDM 2D
                                                    if(flex_is_dm_core())
                                                    {
                                                        flex_dma_sync_2d(local(local_A), hbm_addr(A + ((K * (((64 * ci) + (64 * gi)) + i)) + bK) * 2), 64*2, 64*2, K*2, 64);
                                                    }
                                                    flex_intra_cluster_sync();

                                                    // SoftHier_HBM -> SoftHier_TCDM 2D
                                                    if(flex_is_dm_core())
                                                    {
                                                        flex_dma_sync_2d(local(local_B), hbm_addr(B + ((((N * bK) + (64 * cj)) + (64 * gj)) + j) * 2), 64*2, 64*2, N*2, 64);
                                                    }
                                                    flex_intra_cluster_sync();

                                                    if (flex_is_first_core())
                                                    {
                                                        uint32_t _in_local_a = local_A;
                                                        uint32_t _in_local_b = local_B;
                                                        uint32_t _in_accumulator = accumulator;

                                                        ///////////////////
                                                        flex_redmule_trigger(_in_local_a, _in_local_b, _in_accumulator, REDMULE_FP_16);
                                                        flex_redmule_wait();
                                                        ///////////////////

                                                    }
                                                    flex_intra_cluster_sync();
                                                }
                                            }

                                            // SoftHier_TCDM -> SoftHier_HBM
                                            if(flex_is_dm_core())
                                            {
                                                flex_dma_sync_2d(hbm_addr(C + ((((N * (((64 * ci) + (64 * gi)) + i)) + (64 * cj)) + (64 * gj)) + j) * 2), local(accumulator), 64*2, N*2, 64*2, 64);
                                            }
                                            flex_intra_cluster_sync();
                                        }
                                    }
                                }
                            }
                        }
                    }
                    flex_intra_cluster_sync();
                }
            }
        }
    }
}


void main();
void main()
{
    flex_barrier_xy_init();
    flex_global_barrier_xy();

    // Read the value preloaded into HBM
    uint32_t A = ((uint32_t *)(hbm_addr(0)))[0];
    uint32_t B = ((uint32_t *)(hbm_addr(4)))[0];
    uint32_t C = ((uint32_t *)(hbm_addr(8)))[0];
    uint32_t K = ((uint32_t *)(hbm_addr(12)))[0];
    uint32_t M = ((uint32_t *)(hbm_addr(16)))[0];
    uint32_t N = ((uint32_t *)(hbm_addr(20)))[0];
    uint32_t G = ((uint32_t *)(hbm_addr(24)))[0];

    // address and size
    if (flex_is_first_core() && (flex_get_cluster_id()==0))
    {
        printf("A: %x\n", A);
        printf("B: %x\n", B);
        printf("C: %x\n", C);
        printf("K: %x\n", K);
        printf("M: %x\n", M);
        printf("N: %x\n", N);
        printf("G: %x\n", G);
    }

    // First element of each matrix
    if (flex_is_first_core() && (flex_get_cluster_id()==0))
    {
        printf("%x\n", ((uint16_t *)(hbm_addr(A)))[0]);
        printf("%x\n", ((uint16_t *)(hbm_addr(B)))[0]);
        printf("%x\n", ((uint16_t *)(hbm_addr(C)))[0]);
        printf("%x\n", ((uint16_t *)(hbm_addr(G)))[0]);
    }
    uint32_t eoc_val = 0;
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    gemm(A, B, C, K, M, N);
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    
    // get the output
    if (flex_is_first_core() && (flex_get_cluster_id()==0))
    {
        printf("[Check Results] First 3 Row Elements:\n");
        for (auto C_i = 0; C_i < 3; C_i++) {
            for (auto C_j = 0; C_j < M; C_j++) {
                show_progress_animated(C_j + C_i * M,M*3);
                if (((uint16_t *)(hbm_addr(C)))[C_j + C_i * M] != ((uint16_t *)(hbm_addr(G)))[C_j + C_i * M])
                {
                    eoc_val = 1;
                    printf("\n[Check Error] at (%0d, %0d): expected: 0x%x, but got: 0x%x\n",
                        C_i,
                        C_j,
                        ((uint16_t *)(hbm_addr(G)))[C_j + C_i * M],
                        ((uint16_t *)(hbm_addr(C)))[C_j + C_i * M]);
                    break;
                }
            }
            if (eoc_val == 1)
            {
                break;
            }
        }

        if (eoc_val == 0)
        {
            printf("[Check Passed] GEMM is Numerically Correct\n");
        }
    }
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}

