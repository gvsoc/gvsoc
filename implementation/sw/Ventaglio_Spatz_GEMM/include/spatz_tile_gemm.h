// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// Spatz tile GEMM for SUMMA dataflow. 4-row unrolled, fp16/fp32.
// Computes Z_tile += X_tile * W_tile (accumulating).
// Per K iteration: 1 vle (W row) + 4 fld + 4 vfmacc → amortizes scalar overhead.

#ifndef SPATZ_TILE_GEMM_H
#define SPATZ_TILE_GEMM_H

#include <stdint.h>
#include <stddef.h>
#include "spatz_gemm.h"

#define _STR(x) #x
#define _XSTR(x) _STR(x)

#if DATA_TYPE_WIDTH == 16
typedef uint16_t tile_data_t;
#else
typedef float tile_data_t;
#endif

static inline void spatz_tile_gemm(
    uint32_t z_addr, uint32_t x_addr, uint32_t w_addr,
    uint32_t M_tile, uint32_t K_tile, uint32_t N_tile)
{
    tile_data_t *z = (tile_data_t *)z_addr;
    tile_data_t *x = (tile_data_t *)x_addr;
    tile_data_t *w = (tile_data_t *)w_addr;

    // 4-row unrolled: process 4 M-rows per iteration
    for (uint32_t m = 0; m < M_tile; m += 4) {
        uint32_t p = 0;
        while (p < N_tile) {
            size_t gvl;
            asm volatile("vsetvli %0, %1, e" _XSTR(DATA_TYPE_WIDTH) ", m4, ta, ma"
                         : "=r"(gvl) : "r"(N_tile - p));

            // Load 4 existing Z rows (accumulate)
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(z + (m+0)*N_tile + p) : "memory");
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v4,  (%0)" ::"r"(z + (m+1)*N_tile + p) : "memory");
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(z + (m+2)*N_tile + p) : "memory");
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v12, (%0)" ::"r"(z + (m+3)*N_tile + p) : "memory");

            const tile_data_t *w_col = w + p;
            for (uint32_t k = 0; k < K_tile; k++) {
                // Load W row once
                asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v16, (%0)" ::"r"(w_col) : "memory");

                // 4x FMA with different scalars from 4 X rows
                asm volatile("fld fa0, (%0)" ::"r"(&x[(m+0)*K_tile + k]));
                asm volatile("vfmacc.vf v0, fa0, v16");
                asm volatile("fld fa1, (%0)" ::"r"(&x[(m+1)*K_tile + k]));
                asm volatile("vfmacc.vf v4, fa1, v16");
                asm volatile("fld fa2, (%0)" ::"r"(&x[(m+2)*K_tile + k]));
                asm volatile("vfmacc.vf v8, fa2, v16");
                asm volatile("fld fa3, (%0)" ::"r"(&x[(m+3)*K_tile + k]));
                asm volatile("vfmacc.vf v12, fa3, v16");

                w_col += N_tile;
            }

            // Store 4 result rows
            asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(z + (m+0)*N_tile + p) : "memory");
            asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v4,  (%0)" ::"r"(z + (m+1)*N_tile + p) : "memory");
            asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(z + (m+2)*N_tile + p) : "memory");
            asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v12, (%0)" ::"r"(z + (m+3)*N_tile + p) : "memory");
            p += gvl;
        }
    }
}

#endif // SPATZ_TILE_GEMM_H
