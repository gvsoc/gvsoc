// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// Spatz GEMV tile: z[1 x N_tile] += x[1 x K_tile] * W[K_tile x N_tile]
// Multi-core: split N dimension — each core computes N_tile/num_cores columns.

#ifndef SPATZ_TILE_GEMV_H
#define SPATZ_TILE_GEMV_H

#include <stdint.h>
#include <stddef.h>
#include "spatz_gemm.h"

#define _STR(x) #x
#define _XSTR(x) _STR(x)

#if DATA_TYPE_WIDTH == 16
typedef uint16_t gtile_data_t;
#else
typedef float gtile_data_t;
#endif

static inline void spatz_tile_gemv(
    uint32_t z_addr, uint32_t x_addr, uint32_t w_addr,
    uint32_t K_tile, uint32_t N_tile,
    uint32_t core_id, uint32_t num_cores)
{
    gtile_data_t *z = (gtile_data_t *)z_addr;
    gtile_data_t *x = (gtile_data_t *)x_addr;
    gtile_data_t *w = (gtile_data_t *)w_addr;

    // Split N across cores: each core processes N_tile/num_cores columns
    uint32_t n_per_core = N_tile / num_cores;
    uint32_t n_start = core_id * n_per_core;
    uint32_t n_end = n_start + n_per_core;

    uint32_t p = n_start;
    while (p < n_end) {
        size_t gvl;
        asm volatile("vsetvli %0, %1, e" _XSTR(DATA_TYPE_WIDTH) ", m4, ta, ma"
                     : "=r"(gvl) : "r"(n_end - p));
        asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v0, (%0)" ::"r"(z + p) : "memory");

        const gtile_data_t *w_col = w + p;
        for (uint32_t k = 0; k < K_tile; k++) {
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v16, (%0)" ::"r"(w_col) : "memory");
            asm volatile("fld fa0, (%0)" ::"r"(&x[k]));
            asm volatile("vfmacc.vf v0, fa0, v16");
            w_col += N_tile;  // stride is full N_tile (row-major W)
        }
        asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v0, (%0)" ::"r"(z + p) : "memory");
        p += gvl;
    }
}

#endif // SPATZ_TILE_GEMV_H
