#include "flex_runtime.h"
#include "deepseek_config.h"
#include "vector_lib.h"
// #include "deepseek_moe_gate.h"
// #include "deepseek_moe_scatter.h"
// #include "deepseek_summa_2d.h"
#include "deepseek_kr.h"
#include <math.h>

#define SEQ_LEN 4096
#define HBM_INTERVAL_OFFSET (DEEPSEEK_EMBEDDED_LENGTH * SEQ_LEN)

#define SUMMA_M_TILE 128
#define SUMMA_GROUP_X 16
#define SUMMA_GROUP_Y 32
#define SUMMA_GROUPS  2

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    // DeepSeek_MoE_gate_C32x32(
    //     hbm_south(0,0)/*input_address*/,
    //     hbm_south(0,0)/*weight_address*/,
    //     hbm_south(0,0)/*index_address*/,
    //     hbm_south(0,0)/*score_address*/,
    //     8192/*seq_length*/);

    // DeepSeek_MoE_scatter(
    //     hbm_west(0,0) + 2 * HBM_INTERVAL_OFFSET/*input_address*/,
    //     hbm_west(0,0)/*index_address*/,
    //     hbm_west(0,0) + HBM_INTERVAL_OFFSET/*locat_address*/,
    //     hbm_west(0,0) + 3 * HBM_INTERVAL_OFFSET/*scatter_address*/,
    //     SEQ_LEN/*seq_length*/);

    // DeepSeek_SUMMA_2D(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     SEQ_LEN/*M_size*/,
    //     2048/*N_size*/,
    //     7168/*K_size*/, /*shared dimension*/
    //     SUMMA_M_TILE/*M_tile*/,
    //     128/*N_tile*/,
    //     128/*K_tile*/,
    //     SUMMA_GROUP_X/*group_x*/,
    //     SUMMA_GROUP_Y/*group_y*/,
    //     SUMMA_GROUPS/*num_group*/,
    //     (DEEPSEEK_EMBEDDED_LENGTH * SEQ_LEN * DATA_TYPE_BYTE)/*X_address_group_gap*/,
    //     (DEEPSEEK_EMBEDDED_LENGTH * 2048    * DATA_TYPE_BYTE)/*W_address_group_gap*/,
    //     (DEEPSEEK_EMBEDDED_LENGTH * SEQ_LEN * DATA_TYPE_BYTE)/*Z_address_group_gap*/);

    /*************************************************************************************/
    /*                          Step 1. Latent C^Q + C^KV                                */
    /*************************************************************************************/

    // // 16-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     16/*M_size*/,
    //     2048/*N_size*/,
    //     224/*K_size*/, /*shared dimension*/
    //     16/*M_tile*/,
    //     64/*N_tile*/,
    //     32/*K_tile*/,
    //     32/*group_x*/,
    //     1/*group_y*/,
    //     32/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 32-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     32/*M_size*/,
    //     2048/*N_size*/,
    //     224/*K_size*/, /*shared dimension*/
    //     32/*M_tile*/,
    //     64/*N_tile*/,
    //     32/*K_tile*/,
    //     32/*group_x*/,
    //     1/*group_y*/,
    //     32/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 64-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     64/*M_size*/,
    //     2048/*N_size*/,
    //     448/*K_size*/, /*shared dimension*/
    //     32/*M_tile*/,
    //     64/*N_tile*/,
    //     64/*K_tile*/,
    //     32/*group_x*/,
    //     2/*group_y*/,
    //     16/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 128-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     128/*M_size*/,
    //     2048/*N_size*/,
    //     448/*K_size*/, /*shared dimension*/
    //     64/*M_tile*/,
    //     64/*N_tile*/,
    //     64/*K_tile*/,
    //     32/*group_x*/,
    //     2/*group_y*/,
    //     16/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 256-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     256/*M_size*/,
    //     2048/*N_size*/,
    //     896/*K_size*/, /*shared dimension*/
    //     64/*M_tile*/,
    //     64/*N_tile*/,
    //     128/*K_tile*/,
    //     32/*group_x*/,
    //     4/*group_y*/,
    //     8/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 512-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     512/*M_size*/,
    //     2048/*N_size*/,
    //     896/*K_size*/, /*shared dimension*/
    //     128/*M_tile*/,
    //     64/*N_tile*/,
    //     128/*K_tile*/,
    //     32/*group_x*/,
    //     4/*group_y*/,
    //     8/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 1024-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     1024/*M_size*/,
    //     2048/*N_size*/,
    //     1792/*K_size*/, /*shared dimension*/
    //     128/*M_tile*/,
    //     64/*N_tile*/,
    //     128/*K_tile*/,
    //     32/*group_x*/,
    //     8/*group_y*/,
    //     4/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 2048-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     2048/*M_size*/,
    //     2048/*N_size*/,
    //     1792/*K_size*/, /*shared dimension*/
    //     128/*M_tile*/,
    //     128/*N_tile*/,
    //     128/*K_tile*/,
    //     16/*group_x*/,
    //     16/*group_y*/,
    //     4/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 4096-2048-7168
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     4096/*M_size*/,
    //     2048/*N_size*/,
    //     3584/*K_size*/, /*shared dimension*/
    //     128/*M_tile*/,
    //     128/*N_tile*/,
    //     128/*K_tile*/,
    //     16/*group_x*/,
    //     32/*group_y*/,
    //     2/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    /*************************************************************************************/
    /*                          Step 2. Latent K^R                                      */
    /*************************************************************************************/

    DeepSeek_KR(SEQ_LEN);

    /*************************************************************************************/
    /*                          Step 3. Meta Q^C                                         */
    /*************************************************************************************/

    // if (SEQ_LEN == 512)
    // {
    //     // 512-65536-1536
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         512/*M_size*/,
    //         8192/*N_size*/,
    //         1536/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         4/*group_y*/,
    //         8/*num_group*/,
    //         0/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }
        
    // if (SEQ_LEN == 1024)
    // {
    //     // 1024-65536-1536
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         1024/*M_size*/,
    //         16384/*N_size*/,
    //         1536/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         8/*group_y*/,
    //         4/*num_group*/,
    //         0/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 2048)
    // {
    //     // 2048-65536-1536
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         2048/*M_size*/,
    //         32768/*N_size*/,
    //         1536/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         16/*group_y*/,
    //         2/*num_group*/,
    //         0/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 4096)
    // {
    //     // 4096-65536-1536
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         4096/*M_size*/,
    //         65536/*N_size*/,
    //         1536/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         32/*group_y*/,
    //         1/*num_group*/,
    //         0/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    /*************************************************************************************/
    /*                          Step 4. Meta Q^R                                         */
    /*************************************************************************************/

    // // 512-8192-1536
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     512/*M_size*/,
    //     8192/*N_size*/,
    //     384/*K_size*/, /*shared dimension*/
    //     64/*M_tile*/,
    //     256/*N_tile*/,
    //     128/*K_tile*/,
    //     32/*group_x*/,
    //     8/*group_y*/,
    //     4/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 1024-8192-1536
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     1024/*M_size*/,
    //     8192/*N_size*/,
    //     384/*K_size*/, /*shared dimension*/
    //     128/*M_tile*/,
    //     256/*N_tile*/,
    //     128/*K_tile*/,
    //     32/*group_x*/,
    //     8/*group_y*/,
    //     4/*num_group*/,
    //     1/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    // // 2048-8192-1536
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     2048/*M_size*/,
    //     4096/*N_size*/,
    //     1536/*K_size*/, /*shared dimension*/
    //     128/*M_tile*/,
    //     128/*N_tile*/,
    //     128/*K_tile*/,
    //     32/*group_x*/,
    //     16/*group_y*/,
    //     2/*num_group*/,
    //     0/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);


    // // 4096-8192-1536
    // DeepSeek_SUMMA_2D_Weight_Stationary(
    //     hbm_west(0,0)/*X_address*/,
    //     hbm_south(0,0)/*W_address*/,
    //     hbm_west(0,0)/*Z_address*/,
    //     4096/*M_size*/,
    //     8192/*N_size*/,
    //     1536/*K_size*/, /*shared dimension*/
    //     128/*M_tile*/,
    //     128/*N_tile*/,
    //     128/*K_tile*/,
    //     32/*group_x*/,
    //     32/*group_y*/,
    //     1/*num_group*/,
    //     0/*group_reduction*/,
    //     0/*X_address_group_gap*/,
    //     0/*W_address_group_gap*/,
    //     0/*Z_address_group_gap*/);

    /*************************************************************************************/
    /*                          Step 5. Output_Proj                                      */
    /*************************************************************************************/

    // if (SEQ_LEN == 512)
    // {
    //     // 512-7168-16384
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         512/*M_size*/,
    //         7168/*N_size*/,
    //         2048/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         224/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         4/*group_y*/,
    //         8/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 1024)
    // {
    //     // 1024-7168-16384
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         1024/*M_size*/,
    //         7168/*N_size*/,
    //         4096/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         224/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         8/*group_y*/,
    //         4/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 2048)
    // {
    //     // 2048-7168-16384
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         2048/*M_size*/,
    //         7168/*N_size*/,
    //         8192/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         224/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         16/*group_y*/,
    //         2/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 4096)
    // {
    //     // 4096-7168-16384
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         4096/*M_size*/,
    //         7168/*N_size*/,
    //         16384/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         32/*group_y*/,
    //         1/*num_group*/,
    //         0/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    /*************************************************************************************/
    /*                          Step 6. MoE_Front                                        */
    /*************************************************************************************/

    // if (SEQ_LEN == 512)
    // {
    //     // 2048-4096-7168
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         512/*M_size*/,
    //         4096/*N_size*/,
    //         896/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         4/*group_y*/,
    //         8/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 1024)
    // {
    //     // 2048-4096-7168
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         1024/*M_size*/,
    //         4096/*N_size*/,
    //         1792/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         8/*group_y*/,
    //         4/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 2048)
    // {
    //     // 2048-4096-7168
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         2048/*M_size*/,
    //         4096/*N_size*/,
    //         3584/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         16/*group_y*/,
    //         2/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 4096)
    // {
    //     // 4096-4096-7168
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         4096/*M_size*/,
    //         4096/*N_size*/,
    //         7168/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         32/*group_y*/,
    //         1/*num_group*/,
    //         0/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    /*************************************************************************************/
    /*                          Step 7. MoE_Back                                         */
    /*************************************************************************************/

    // if (SEQ_LEN == 512)
    // {
    //     // 512-7168-2048
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         512/*M_size*/,
    //         7168/*N_size*/,
    //         256/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         224/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         4/*group_y*/,
    //         8/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 1024)
    // {
    //     // 1024-7168-2048
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         1024/*M_size*/,
    //         7168/*N_size*/,
    //         512/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         224/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         8/*group_y*/,
    //         4/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 2048)
    // {
    //     // 2048-7168-2048
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         2048/*M_size*/,
    //         7168/*N_size*/,
    //         1024/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         224/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         16/*group_y*/,
    //         2/*num_group*/,
    //         1/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }

    // if (SEQ_LEN == 4096)
    // {
    //     // 4096-7168-2048
    //     DeepSeek_SUMMA_2D_Weight_Stationary(
    //         hbm_west(0,0)/*X_address*/,
    //         hbm_south(0,0)/*W_address*/,
    //         hbm_west(0,0)/*Z_address*/,
    //         4096/*M_size*/,
    //         7168/*N_size*/,
    //         2048/*K_size*/, /*shared dimension*/
    //         128/*M_tile*/,
    //         128/*N_tile*/,
    //         128/*K_tile*/,
    //         32/*group_x*/,
    //         32/*group_y*/,
    //         1/*num_group*/,
    //         0/*group_reduction*/,
    //         0/*X_address_group_gap*/,
    //         0/*W_address_group_gap*/,
    //         0/*Z_address_group_gap*/);
    // }


    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}