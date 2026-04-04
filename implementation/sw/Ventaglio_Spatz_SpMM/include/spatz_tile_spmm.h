// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// Spatz sparse tile GEMM for SUMMA. 4-row unrolled, multi-core.

#ifndef SPATZ_TILE_SPMM_H
#define SPATZ_TILE_SPMM_H

#include <stdint.h>
#include <stddef.h>
#include "spatz_spmm.h"

#define _STR(x) #x
#define _XSTR(x) _STR(x)

#if DATA_TYPE_WIDTH == 16
typedef uint16_t stile_data_t;
#else
typedef float stile_data_t;
#endif

#define TILE_ASM_VLX32_V20(rs1_var) \
    asm volatile(".insn r 0x07, 6, 0x09, x20, %0, x0" ::"r"(rs1_var) : "memory")
#define TILE_ASM_VFXMACC_V0_FA0  asm volatile(".insn r 0x57, 5, 0x4B, x0, x10, x16")
#define TILE_ASM_VFXMACC_V4_FA1  asm volatile(".insn r 0x57, 5, 0x4B, x4, x11, x16")
#define TILE_ASM_VFXMACC_V8_FA2  asm volatile(".insn r 0x57, 5, 0x4B, x8, x12, x16")
#define TILE_ASM_VFXMACC_V12_FA3 asm volatile(".insn r 0x57, 5, 0x4B, x12, x13, x16")

static inline void spatz_tile_spmm(
    uint32_t z_addr, uint32_t x_addr, uint32_t w_addr, uint32_t idx_addr,
    uint32_t M_tile, uint32_t K_tile, uint32_t N_tile,
    uint32_t core_id, uint32_t num_cores)
{
    uint32_t PW_tile = N_tile * SPMM_N_SPARSE / SPMM_M_SPARSE;

    stile_data_t *z = (stile_data_t *)z_addr;
    stile_data_t *x = (stile_data_t *)x_addr;
    stile_data_t *w = (stile_data_t *)w_addr;
    uint32_t *idx = (uint32_t *)idx_addr;

    uint32_t m_per_core = M_tile / num_cores;
    uint32_t m_start = core_id * m_per_core;
    uint32_t m_end = m_start + m_per_core;

    for (uint32_t m = m_start; m < m_end; m += 4) {
        uint32_t p = 0;
        while (p < PW_tile) {
            size_t gvl;
            asm volatile("vsetvli %0, %1, e" _XSTR(DATA_TYPE_WIDTH) ", m4, ta, ma"
                         : "=r"(gvl) : "r"(PW_tile - p));

            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(z + (m+0)*PW_tile + p) : "memory");
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v4,  (%0)" ::"r"(z + (m+1)*PW_tile + p) : "memory");
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(z + (m+2)*PW_tile + p) : "memory");
            asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v12, (%0)" ::"r"(z + (m+3)*PW_tile + p) : "memory");

            const stile_data_t *w_col = w + p;
            const uint32_t *idx_col = idx + p;
            for (uint32_t k = 0; k < K_tile; k++) {
                TILE_ASM_VLX32_V20(idx_col);
                asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v16, (%0)" ::"r"(w_col) : "memory");
                asm volatile("fld fa0, (%0)" ::"r"(&x[(m+0)*K_tile + k]));
                TILE_ASM_VFXMACC_V0_FA0;
                asm volatile("fld fa1, (%0)" ::"r"(&x[(m+1)*K_tile + k]));
                TILE_ASM_VFXMACC_V4_FA1;
                asm volatile("fld fa2, (%0)" ::"r"(&x[(m+2)*K_tile + k]));
                TILE_ASM_VFXMACC_V8_FA2;
                asm volatile("fld fa3, (%0)" ::"r"(&x[(m+3)*K_tile + k]));
                TILE_ASM_VFXMACC_V12_FA3;
                w_col += PW_tile;
                idx_col += PW_tile;
            }

            asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v0,  (%0)" ::"r"(z + (m+0)*PW_tile + p) : "memory");
            asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v4,  (%0)" ::"r"(z + (m+1)*PW_tile + p) : "memory");
            asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(z + (m+2)*PW_tile + p) : "memory");
            asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v12, (%0)" ::"r"(z + (m+3)*PW_tile + p) : "memory");
            p += gvl;
        }
    }
}

#endif // SPATZ_TILE_SPMM_H
