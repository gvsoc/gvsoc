// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// Spatz GEMV tile for SUMMA: z[1 x N_tile] += x[1 x K_tile] * W[K_tile x N_tile]
// Single-row: no M-unrolling. K-unrolling instead — process multiple K per vfmacc.

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

// GEMV tile: z[N_tile] += x[K_tile] · W[K_tile x N_tile]
static inline void spatz_tile_gemv(
    uint32_t z_addr, uint32_t x_addr, uint32_t w_addr,
    uint32_t K_tile, uint32_t N_tile)
{
    gtile_data_t *z = (gtile_data_t *)z_addr;
    gtile_data_t *x = (gtile_data_t *)x_addr;
    gtile_data_t *w = (gtile_data_t *)w_addr;

    uint32_t p = 0;
    while (p < N_tile) {
        size_t gvl;
        asm volatile("vsetvli %0, %1, e" _XSTR(DATA_TYPE_WIDTH) ", m4, ta, ma"
                     : "=r"(gvl) : "r"(N_tile - p));

        // Load existing z (accumulate)
        asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v0, (%0)" ::"r"(z + p) : "memory");

        // K-loop: accumulate x[k] * W[k, p:p+gvl]
        const gtile_data_t *w_col = w + p;
        for (uint32_t k = 0; k < K_tile; k++) {
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v16, (%0)" ::"r"(w_col) : "memory");
            asm volatile("fld fa0, (%0)" ::"r"(&x[k]));
            asm volatile("vfmacc.vf v0, fa0, v16");
            w_col += N_tile;
        }

        asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v0, (%0)" ::"r"(z + p) : "memory");
        p += gvl;
    }
}

#endif // SPATZ_TILE_GEMV_H
