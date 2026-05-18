#ifndef _GEMM_SYSTOLIC_WISE_DOL_H_
#define _GEMM_SYSTOLIC_WISE_DOL_H_

#include <math.h>
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_printf.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

/*
 * DoL (DRAM-on-Logic) address helpers.
 *
 * Each cluster (tx, ty) owns a private HBM bank of ARCH_HBM_NODE_ADDR_SPACE bytes.
 * The flat address of cluster (tx,ty)'s bank is:
 *   ARCH_HBM_START_BASE + (ty*NUM_X + tx) * ARCH_HBM_NODE_ADDR_SPACE
 */

#define hbm_dol(tx, ty, offset) \
    ((uint64_t)ARCH_HBM_START_BASE + \
     ((uint64_t)(ty) * ARCH_NUM_CLUSTER_X + (uint64_t)(tx)) * (uint64_t)ARCH_HBM_NODE_ADDR_SPACE + \
     (uint64_t)(offset))

// Set this to 1 to replicate input data across all tiles' HBM banks, enabling fully local access at the cost of extra storage
#define INPUT_DATA_REPLICATION 0

/*
 * Data layout within each tile's HBM bank (consistent across all tiles):
 *
 *  [0,                    x_tiles*tile_X)                  — X section
 *  [x_tiles*tile_X,       x_tiles*tile_X + w_tiles*tile_W) — W section
 *  [...,                  ...+Z_tile_all*tile_Y)            — Y section
 *
 * where x_tiles = Z_tile_on_col * XW_tile_length   (unique X tiles: m_idx x n_tile)
 *       w_tiles = Z_tile_on_row * XW_tile_length   (unique W tiles: k_idx x n_tile)
 *
 * X is indexed by (m_idx, n_tile); k_idx is DROPPED because all k_idx values for the
 * same (m_idx, n_tile) access the identical X data.  Likewise W is indexed by (k_idx,
 * n_tile); m_idx is dropped.
 *
 * Without INPUT_DATA_REPLICATION (systolic pass-through):
 *   X section: populated only in tiles (0, y)  — west-edge column.
 *   W section: populated only in tiles (x, 0)  — south-edge row.
 *   Preloader: X[m-slice y] -> hbm_dol(0, y, 0),  W[k-slice x] -> hbm_dol(x, 0, w_section).
 *
 * With INPUT_DATA_REPLICATION (fully local):
 *   Every tile (x, y) holds its own copy of X[m-slice y] and W[k-slice x].
 *   Preloader: for each (x,y): X[m-slice y] -> hbm_dol(x, y, 0),
 *                              W[k-slice x] -> hbm_dol(x, y, w_section).
 *   Y output is written at runtime; no preload needed in either variant.
 */

typedef struct GemmSystolicInfo_DoL
{
    // General information
    uint32_t matrix_M;
    uint32_t matrix_N;
    uint32_t matrix_K;

    // Tile information
    uint32_t tile_dimension_M;
    uint32_t tile_dimension_N;
    uint32_t tile_dimension_K;
    uint32_t elem_size;
    uint32_t tile_size_byte_X;
    uint32_t tile_size_byte_W;
    uint32_t tile_size_byte_Y;

    // L1 double-buffer layout in TCDM: X1 | W1 | X2 | W2 | Y
    uint32_t X_offset_1;
    uint32_t W_offset_1;
    uint32_t X_offset_2;
    uint32_t W_offset_2;
    uint32_t Y_offset;

    // Iteration counters
    uint32_t XW_tile_length;   // number of N-tiles (inner-product steps per output Y-tile)
    uint32_t Z_tile_on_row;    // K-output tiles owned by this cluster: K positions {pos.x, pos.x+NUM_X, ...}
    uint32_t Z_tile_on_col;    // M-output tiles owned by this cluster: M positions {pos.y, pos.y+NUM_Y, ...}
    uint32_t Z_tile_all;       // total output Y-tiles for this cluster = Z_tile_on_row * Z_tile_on_col
    uint32_t systolic_delay;   // pos.x + pos.y: stagger so left/bottom neighbors have written TCDM before this cluster reads it
    uint32_t total_iter;

    // In-flight flags
    uint32_t dma_runing;
    uint32_t redmule_runing;

    // Pre-computed actions for the current global iteration (filled by compute_dma_access / compute_redmule_action)
    uint32_t use_dma1;
    uint64_t dma1_src;
    uint64_t dma1_dst;
    uint32_t dma1_size;

    uint32_t use_dma2;
    uint64_t dma2_src;
    uint64_t dma2_dst;
    uint32_t dma2_size;

    uint32_t use_sync_dma;     // forces serial dma1→dma2 (Y store must finish before zero-fill touches the same buffer)

    uint32_t use_redmule;
    uint32_t redmule_x;
    uint32_t redmule_w;
    uint32_t redmule_y;

    // DoL HBM section base addresses (absolute, 64-bit).
    // Without replication: x in tile (0, pos.y), w in tile (pos.x, 0).
    // With INPUT_DATA_REPLICATION: both in own tile (pos.x, pos.y).
    uint64_t x_hbm_section;
    uint64_t w_hbm_section;
    uint64_t y_hbm_section;    // always in own tile (pos.x, pos.y)
} GemmSystolicInfo_DoL;


GemmSystolicInfo_DoL gemm_systolic_wise_DoL_analysis(
    uint32_t M_size, uint32_t N_size, uint32_t K_size, uint32_t elem_size,
    uint32_t tile_dimension_M, uint32_t tile_dimension_N, uint32_t tile_dimension_K)
{
    GemmSystolicInfo_DoL info;
    info.matrix_M = M_size;
    info.matrix_N = N_size;
    info.matrix_K = K_size;
    info.tile_dimension_M = tile_dimension_M;
    info.tile_dimension_N = tile_dimension_N;
    info.tile_dimension_K = tile_dimension_K;
    info.elem_size = elem_size;
    info.tile_size_byte_X = info.tile_dimension_M * info.tile_dimension_N * elem_size;
    info.tile_size_byte_W = info.tile_dimension_N * info.tile_dimension_K * elem_size;
    info.tile_size_byte_Y = info.tile_dimension_M * info.tile_dimension_K * elem_size;

    info.X_offset_1 = 0;
    info.W_offset_1 = info.tile_size_byte_X;
    info.X_offset_2 = info.tile_size_byte_X + info.tile_size_byte_W;
    info.W_offset_2 = 2 * info.tile_size_byte_X + info.tile_size_byte_W;
    info.Y_offset   = 2 * (info.tile_size_byte_X + info.tile_size_byte_W);

    uint32_t M_tile = (M_size + info.tile_dimension_M - 1)/info.tile_dimension_M;   // tiles needed to cover M dimension
    uint32_t N_tile = (N_size + info.tile_dimension_N - 1)/info.tile_dimension_N;   // tiles needed to cover N dimension
    uint32_t K_tile = (K_size + info.tile_dimension_K - 1)/info.tile_dimension_K;   // tiles needed to cover K dimension

    FlexPosition pos = get_pos(flex_get_cluster_id());

    // Count K-output tiles owned by this cluster: pos.x, pos.x+NUM_X, pos.x+2*NUM_X, ...
    info.Z_tile_on_row = 0;
    while ((info.Z_tile_on_row * ARCH_NUM_CLUSTER_X + pos.x) < K_tile){
        info.Z_tile_on_row++;
    }
    // Count M-output tiles owned by this cluster: pos.y, pos.y+NUM_Y, pos.y+2*NUM_Y, ...
    info.Z_tile_on_col = 0;
    while((info.Z_tile_on_col * ARCH_NUM_CLUSTER_Y + pos.y) < M_tile){
        info.Z_tile_on_col++;
    }
    info.XW_tile_length = N_tile;
    info.Z_tile_all     = info.Z_tile_on_row * info.Z_tile_on_col;
    // ARCH_NUM_CLUSTER_X + ARCH_NUM_CLUSTER_Y extra iterations drain the systolic pipeline
    // (max delay = (NUM_X-1)+(NUM_Y-1), so all clusters finish before total_iter expires)
    info.total_iter     = ((K_tile + ARCH_NUM_CLUSTER_X - 1) / ARCH_NUM_CLUSTER_X)
                        * ((M_tile + ARCH_NUM_CLUSTER_Y - 1) / ARCH_NUM_CLUSTER_Y)
                        * (N_tile + 1)
                        + ARCH_NUM_CLUSTER_X + ARCH_NUM_CLUSTER_Y;

    info.dma_runing    = 0;
    info.redmule_runing = 0;

    // Compact HBM layout sizes:
    // x_tiles: unique X tiles = Z_tile_on_col * N_tile  (k_idx dropped — X[m,n] is same for all k)
    // w_tiles: unique W tiles = Z_tile_on_row * N_tile  (m_idx dropped — W[k,n] is same for all m)
    uint64_t x_tiles   = (uint64_t)info.Z_tile_on_col * info.XW_tile_length;
    uint64_t w_tiles   = (uint64_t)info.Z_tile_on_row * info.XW_tile_length;
    uint64_t w_section = x_tiles * info.tile_size_byte_X;          // byte offset where W data starts in the HBM bank
    uint64_t y_section = w_section + w_tiles * info.tile_size_byte_W;  // byte offset where Y data starts in the HBM bank

#if INPUT_DATA_REPLICATION
    // Every tile holds its own copy of the X and W slices it needs.
    // No TCDM pass-through: all tiles start simultaneously (delay = 0).
    info.systolic_delay = 0;
    info.x_hbm_section = hbm_dol(pos.x, pos.y, 0);          // X in own tile
    info.w_hbm_section = hbm_dol(pos.x, pos.y, w_section);   // W in own tile
#else
    // Systolic pass-through: X flows east from the west-edge tile, W flows north from the south-edge tile.
    // Each hop takes one global iteration, so pos.x + pos.y stagger guarantees data arrives in time.
    info.systolic_delay = pos.x + pos.y;
    info.x_hbm_section = hbm_dol(0,     pos.y, 0);           // X stored only in tile (0, pos.y) — west-edge column
    info.w_hbm_section = hbm_dol(pos.x, 0,     w_section);   // W stored only in tile (pos.x, 0) — south-edge row
#endif
    info.y_hbm_section = hbm_dol(pos.x, pos.y, y_section);   // Y always written to this cluster's own HBM bank

    return info;
}


void gemm_systolic_wise_DoL_compute_dma_access(GemmSystolicInfo_DoL *info, uint32_t iter)
{
    info->use_dma1     = 0;
    info->use_dma2     = 0;
    info->use_sync_dma = 0;

    // Active window: [systolic_delay, systolic_delay + Z_tile_all*(XW_tile_length+1))
    if ((iter >= info->systolic_delay) &&
        (iter < (info->Z_tile_all * (info->XW_tile_length + 1) + info->systolic_delay)))
    {
        uint32_t eff_iter = iter - info->systolic_delay;

        // sub_iter cycles 0..XW_tile_length within each output Y-tile:
        //   0            prefetch the first X/W pair of the upcoming st_count (n_tile = 0)
        //   1            flush finished Y tile to HBM, zero the TCDM accumulator
        //   2..N_tile    load next X/W pair for RedMuLE to accumulate into Y
        uint32_t sub_iter = eff_iter % (info->XW_tile_length + 1);

        // st_count: which output Y-tile is being computed (0 .. Z_tile_all-1).
        // Maps to a unique (m_idx, k_idx) pair identifying the M-row and K-column of Y.
        uint32_t st_count = eff_iter / (info->XW_tile_length + 1);

        // xw_count: linear DMA step index into the X/W transfer sequence.
        // For sub_iter=0 (prefetch), it points at step 0 of the current st_count (n_tile=0).
        // For sub_iter>=2, it advances by (sub_iter-1) steps within that st_count.
        uint32_t xw_count = (sub_iter < 1)
                          ? st_count * info->XW_tile_length
                          : st_count * info->XW_tile_length + sub_iter - 1;
        FlexPosition pos = get_pos(flex_get_cluster_id());

        if (sub_iter == 1)
        {
            // Y accumulator for st_count-1 is complete; flush it to HBM and reset the buffer.
            // Skip at st_count=0: no Y tile has been fully computed yet.
            if (st_count != 0)
            {
                // Serial DMA: store must finish before zero-fill overwrites the same TCDM buffer.
                info->use_sync_dma = 1;
                info->use_dma1  = 1;
                // st_count-1: flush the Y tile from the PREVIOUS output block, not the current one
                info->dma1_dst  = info->y_hbm_section + (uint64_t)(st_count - 1) * info->tile_size_byte_Y;
                info->dma1_src  = local(info->Y_offset);
                info->dma1_size = info->tile_size_byte_Y;
                info->use_dma2  = 1;
                info->dma2_dst  = local(info->Y_offset);
                info->dma2_src  = zomem(0);    // zero-memory: always reads as 0, used to reset the Y TCDM accumulator
                info->dma2_size = info->tile_size_byte_Y;
            }
        } else {
            info->use_dma1  = 1;
            info->use_dma2  = 1;
            info->dma1_size = info->tile_size_byte_X;
            info->dma2_size = info->tile_size_byte_W;

            // Double-buffer toggle: alternate TCDM slots so RedMuLE can read the previous tile while DMA loads the next
            uint32_t local_x = (xw_count % 2 == 0) ? info->X_offset_1 : info->X_offset_2;
            uint32_t local_w = (xw_count % 2 == 0) ? info->W_offset_1 : info->W_offset_2;

            // Compact HBM index for X: m_idx selects the M-block; k_idx is dropped because
            // X[m, n] is identical for all k_idx values that share the same st_count group.
            uint32_t m_idx  = st_count / info->Z_tile_on_row;
            // Compact HBM index for W: k_idx selects the K-block; m_idx is dropped because
            // W[k, n] is identical for all m_idx values that share the same st_count group.
            uint32_t k_idx  = st_count % info->Z_tile_on_row;
            // n_tile: inner-product step (0..XW_tile_length-1), extracted from the linear xw_count
            uint32_t n_tile = xw_count - st_count * info->XW_tile_length;

#if INPUT_DATA_REPLICATION
            // Every tile loads X and W directly from its own local HBM bank.
            info->dma1_dst = local(local_x);
            info->dma1_src = info->x_hbm_section + (uint64_t)(m_idx * info->XW_tile_length + n_tile) * info->tile_size_byte_X;
            info->dma2_dst = local(local_w);
            info->dma2_src = info->w_hbm_section + (uint64_t)(k_idx * info->XW_tile_length + n_tile) * info->tile_size_byte_W;
#else
            // X tile: west-edge clusters (pos.x == 0) pull from their own local HBM;
            // all other clusters read from the left neighbor's TCDM (systolic east-flow).
            if (pos.x == 0) {
                info->dma1_dst = local(local_x);
                info->dma1_src = info->x_hbm_section + (uint64_t)(m_idx * info->XW_tile_length + n_tile) * info->tile_size_byte_X;
            } else {
                // TCDM pass-through: the left neighbor already holds this tile in the same buffer slot
                info->dma1_dst = local(local_x);
                info->dma1_src = remote_pos(left_pos(pos), local_x);
            }

            // W tile: south-edge clusters (pos.y == 0) pull from their own local HBM;
            // all other clusters read from the bottom neighbor's TCDM (systolic north-flow).
            if (pos.y == 0) {
                info->dma2_dst = local(local_w);
                info->dma2_src = info->w_hbm_section + (uint64_t)(k_idx * info->XW_tile_length + n_tile) * info->tile_size_byte_W;
            } else {
                // TCDM pass-through: the bottom neighbor already holds this tile in the same buffer slot
                info->dma2_dst = local(local_w);
                info->dma2_src = remote_pos(bottom_pos(pos), local_w);
            }
#endif
        }
    }

    // Final store: the last Y tile (st_count = Z_tile_all) has no sub_iter=1 successor inside
    // the main loop, so it is flushed one iteration after the active window closes.
    if (iter == (info->Z_tile_all * (info->XW_tile_length + 1) + info->systolic_delay + 1))
    {
        uint32_t eff_iter = iter - info->systolic_delay;
        uint32_t st_count = eff_iter / (info->XW_tile_length + 1);   // = Z_tile_all

        info->use_dma1  = 1;
        info->dma1_dst  = info->y_hbm_section + (uint64_t)(st_count - 1) * info->tile_size_byte_Y;
        info->dma1_src  = local(info->Y_offset);
        info->dma1_size = info->tile_size_byte_Y;
    }
}


void gemm_systolic_wise_DoL_compute_redmule_action(GemmSystolicInfo_DoL *info, uint32_t iter)
{
    info->use_redmule = 0;

    // RedMuLE lags the DMA by one global iteration: it computes on data the DMA loaded in (iter-1).
    // The active window is therefore shifted by +1 relative to the DMA active window.
    if ((iter >= (info->systolic_delay + 1)) &&
        (iter < (info->Z_tile_all * (info->XW_tile_length + 1) + info->systolic_delay + 1)))
    {
        uint32_t eff_iter = iter - info->systolic_delay - 1;
        uint32_t sub_iter = eff_iter % (info->XW_tile_length + 1);
        uint32_t st_count = eff_iter / (info->XW_tile_length + 1);
        uint32_t xw_count = (sub_iter < 1)
                          ? st_count * info->XW_tile_length
                          : st_count * info->XW_tile_length + sub_iter - 1;
        // Skip sub_iter=1: no X/W data was loaded in that DMA step, so there is no GEMM tile to run
        if (sub_iter != 1)
        {
            info->use_redmule = 1;
            info->redmule_x   = (xw_count % 2 == 0) ? info->X_offset_1 : info->X_offset_2;
            info->redmule_w   = (xw_count % 2 == 0) ? info->W_offset_1 : info->W_offset_2;
            info->redmule_y   = info->Y_offset;
        }
    }
}


void gemm_systolic_wise_DoL(
    uint32_t M_size, uint32_t N_size, uint32_t K_size, uint32_t elem_size,
    uint32_t tile_dimension_M, uint32_t tile_dimension_N, uint32_t tile_dimension_K)
{
    // Barrier ensures all clusters begin from a consistent state before the analysis phase
    flex_global_barrier_xy();
    GemmSystolicInfo_DoL info = gemm_systolic_wise_DoL_analysis(
        M_size, N_size, K_size, elem_size,
        tile_dimension_M, tile_dimension_N, tile_dimension_K);

    if (flex_is_first_core())
    {
        flex_redmule_config(info.tile_dimension_M, info.tile_dimension_N, info.tile_dimension_K);
        // Pre-compute iteration 0 so the loop body can fire immediately without a stall on the first cycle
        gemm_systolic_wise_DoL_compute_redmule_action(&info, 0);
    }

    if (flex_is_dm_core())
    {
        // Pre-compute iteration 0 so the loop body can fire immediately without a stall on the first cycle
        gemm_systolic_wise_DoL_compute_dma_access(&info, 0);
    }

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();

    for (int i = 0; i < info.total_iter; ++i)
    {
        if (flex_is_first_core())
        {
            // Fire the pre-computed RedMuLE action asynchronously
            if (info.use_redmule) flex_redmule_trigger(info.redmule_x, info.redmule_w, info.redmule_y, REDMULE_NONE_16);
            info.redmule_runing = info.use_redmule;

            // Overlap: pre-compute next RedMuLE action while the current one runs
            gemm_systolic_wise_DoL_compute_redmule_action(&info, i + 1);

            if (info.redmule_runing) flex_redmule_wait();
        }

        if (flex_is_dm_core())
        {
            if (info.use_sync_dma)
            {
                // Y-store step: flush then zero-fill serially to avoid RAW hazard on the Y buffer
                if (info.use_dma1) { flex_dma_async_1d(info.dma1_dst, info.dma1_src, info.dma1_size); flex_dma_async_wait_all(); }
                if (info.use_dma2) { flex_dma_async_1d(info.dma2_dst, info.dma2_src, info.dma2_size); flex_dma_async_wait_all(); }
                gemm_systolic_wise_DoL_compute_dma_access(&info, i + 1);
            } else {
                // X/W load step: fire both DMAs concurrently, then pre-compute next action while they run
                if (info.use_dma1) { flex_dma_async_1d(info.dma1_dst, info.dma1_src, info.dma1_size); }
                if (info.use_dma2) { flex_dma_async_1d(info.dma2_dst, info.dma2_src, info.dma2_size); }
                info.dma_runing = info.use_dma1 | info.use_dma2;

                // Overlap: pre-compute next DMA action while current transfers run
                gemm_systolic_wise_DoL_compute_dma_access(&info, i + 1);

                if (info.dma_runing) flex_dma_async_wait_all();
            }
        }

        flex_global_barrier_xy();
    }
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
}


#endif
