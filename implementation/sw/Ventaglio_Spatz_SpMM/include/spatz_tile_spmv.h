// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// Spatz SpMV tile for SUMMA: z[1 x PW_tile] += x[1 x K_tile] * W_compact[K_tile x PW_tile]
// Single-row with Ventaglio sparse instructions.

#ifndef SPATZ_TILE_SPMV_H
#define SPATZ_TILE_SPMV_H

#include <stdint.h>
#include <stddef.h>
#include "spatz_spmm.h"

#define _STR(x) #x
#define _XSTR(x) _STR(x)

#if DATA_TYPE_WIDTH == 16
typedef uint16_t svtile_data_t;
#else
typedef float svtile_data_t;
#endif

// vlx32.v v20, (rs1)
#define SPMV_ASM_VLX32_V20(rs1_var) \
    asm volatile(".insn r 0x07, 6, 0x09, x20, %0, x0" ::"r"(rs1_var) : "memory")

// vfxmacc.vf v0, fa0, v16
#define SPMV_ASM_VFXMACC_V0_FA0 asm volatile(".insn r 0x57, 5, 0x4B, x0, x10, x16")

// SpMV tile: z[PW_tile] += x[K_tile] · W_compact[K_tile x PW_tile]
static inline void spatz_tile_spmv(
    uint32_t z_addr, uint32_t x_addr, uint32_t w_addr, uint32_t idx_addr,
    uint32_t K_tile, uint32_t N_tile)
{
    uint32_t PW_tile = N_tile * SPMM_N_SPARSE / SPMM_M_SPARSE;

    svtile_data_t *z = (svtile_data_t *)z_addr;
    svtile_data_t *x = (svtile_data_t *)x_addr;
    svtile_data_t *w = (svtile_data_t *)w_addr;
    uint32_t *idx = (uint32_t *)idx_addr;

    uint32_t p = 0;
    while (p < PW_tile) {
        size_t gvl;
        asm volatile("vsetvli %0, %1, e" _XSTR(DATA_TYPE_WIDTH) ", m4, ta, ma"
                     : "=r"(gvl) : "r"(PW_tile - p));

        // Load existing z (accumulate)
        asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v0, (%0)" ::"r"(z + p) : "memory");

        const svtile_data_t *w_col = w + p;
        const uint32_t *idx_col = idx + p;
        for (uint32_t k = 0; k < K_tile; k++) {
            SPMV_ASM_VLX32_V20(idx_col);
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v16, (%0)" ::"r"(w_col) : "memory");
            asm volatile("fld fa0, (%0)" ::"r"(&x[k]));
            SPMV_ASM_VFXMACC_V0_FA0;
            w_col += PW_tile;
            idx_col += PW_tile;
        }

        asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v0, (%0)" ::"r"(z + p) : "memory");
        p += gvl;
    }
}

#endif // SPATZ_TILE_SPMV_H
