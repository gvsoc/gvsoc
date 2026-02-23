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