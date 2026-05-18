#ifndef _TEST_DOL_CHANNEL_H_
#define _TEST_DOL_CHANNEL_H_

#include "flex_runtime.h"
#include "flex_dma_pattern.h"
#include "flex_cluster_arch.h"

/* Each DoL tile maps to one HBM channel; channel stride = ARCH_HBM_NODE_ADDR_SPACE */
#define hbm_dol(tx, ty, offset) \
    ((uint64_t)ARCH_HBM_START_BASE + \
     ((uint64_t)(ty) * ARCH_NUM_CLUSTER_X + (uint64_t)(tx)) * \
     (uint64_t)ARCH_HBM_NODE_ADDR_SPACE + (uint64_t)(offset))

#define TEST_TRANSFER_SIZE 64
#define PARALLEL_EXEC 1

static inline void test_dol_channel_all()
{
    uint32_t     cid     = flex_get_cluster_id();
    FlexPosition pos     = get_pos(cid);
    uint8_t      pattern = (uint8_t)cid+1; // Avoid pattern 0 for better debug

#if PARALLEL_EXEC
    if (flex_is_dm_core()) {
        volatile uint8_t *write_buf = (volatile uint8_t *)local(0x000);
        uint64_t          hbm_base  = hbm_dol(pos.x, pos.y, 0);

        for (int i = 0; i < TEST_TRANSFER_SIZE; i++)
            write_buf[i] = pattern;

        flex_dma_async_1d(hbm_base, (uint64_t)local(0x000), TEST_TRANSFER_SIZE);
        flex_dma_async_wait_all();

        flex_dma_async_1d((uint64_t)local(0x100), hbm_base, TEST_TRANSFER_SIZE);
        flex_dma_async_wait_all();
    }
    flex_global_barrier_xy();
#else
    for (int c = 0; c < ARCH_NUM_CLUSTER; c++) {
        if (flex_is_dm_core() && cid == (uint32_t)c) {
            volatile uint8_t *write_buf = (volatile uint8_t *)local(0x000);
            uint64_t          hbm_base  = hbm_dol(pos.x, pos.y, 0);

            for (int i = 0; i < TEST_TRANSFER_SIZE; i++)
                write_buf[i] = pattern;

            flex_dma_async_1d(hbm_base, (uint64_t)local(0x000), TEST_TRANSFER_SIZE);
            printf("Writing to HBM channel %d using address 0x%08x%08x\n",
                   cid, (uint32_t)(hbm_base >> 32), (uint32_t)hbm_base);
            flex_dma_async_wait_all();

            flex_dma_async_1d((uint64_t)local(0x100), hbm_base, TEST_TRANSFER_SIZE);
            flex_dma_async_wait_all();
        }
        flex_global_barrier_xy();
    }
#endif

    for (int c = 0; c < ARCH_NUM_CLUSTER; c++) {
        if (flex_is_first_core() && cid == (uint32_t)c) {
            volatile uint8_t *read_buf = (volatile uint8_t *)local(0x100);
            int pass = 1;
            uint8_t error_pattern = pattern;
            for (int i = 0; i < TEST_TRANSFER_SIZE; i++) {
                if (read_buf[i] != pattern) {
                    pass = 0;
                    error_pattern = read_buf[i];
                    break;
                }
            }
            printf("[Tile (%d,%d) node=%2d pattern=0x%02x read=0x%02x] %s\n",
                   pos.x, pos.y, cid, (uint32_t)pattern, (uint32_t)error_pattern,
                   pass ? "PASS" : "FAIL");
        }
        flex_global_barrier_xy();
    }
}

#endif /* _TEST_DOL_CHANNEL_H_ */
