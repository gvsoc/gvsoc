// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// N:M SpMM kernel with 4-row unrolling.
// Supports fp16 and fp32 via DATA_TYPE_WIDTH macro.
// Per K iteration: 1 vlx (index) + 1 vle (weights) + 4 vfxmacc (one per row).

#ifndef SP_FSPMM_H
#define SP_FSPMM_H

#include <stddef.h>
#include <stdint.h>

#ifndef DATA_TYPE_WIDTH
#define DATA_TYPE_WIDTH 32
#endif
#ifndef DATA_TYPE_BYTE
#define DATA_TYPE_BYTE (DATA_TYPE_WIDTH / 8)
#endif

#define _STR(x) #x
#define _XSTR(x) _STR(x)

#if DATA_TYPE_WIDTH == 16
typedef uint16_t spmm_data_t;
#elif DATA_TYPE_WIDTH == 32
typedef float spmm_data_t;
#endif

// vlx32.v v20, (rs1) — load indices
#define ASM_VLX32_V20(rs1_var)                                      \
    asm volatile(".insn r 0x07, 6, 0x09, x20, %0, x0"              \
                 ::"r"(rs1_var) : "memory")

// vfxmacc.vf vd, frs1, v16 — sparse FMA (hardcoded frs1 register)
#define ASM_VFXMACC_V0_FA0  asm volatile(".insn r 0x57, 5, 0x4B, x0, x10, x16")
#define ASM_VFXMACC_V4_FA1  asm volatile(".insn r 0x57, 5, 0x4B, x4, x11, x16")
#define ASM_VFXMACC_V8_FA2  asm volatile(".insn r 0x57, 5, 0x4B, x8, x12, x16")
#define ASM_VFXMACC_V12_FA3 asm volatile(".insn r 0x57, 5, 0x4B, x12, x13, x16")

inline void spmm_4xVL(spmm_data_t *c, const spmm_data_t *a, const spmm_data_t *w,
                       const uint32_t *idx,
                       const unsigned int m_start, const unsigned int m_end,
                       const unsigned int K, const unsigned int N,
                       const unsigned int P_W,
                       const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

inline void spmm_4xVL(spmm_data_t *c, const spmm_data_t *a, const spmm_data_t *w,
                       const uint32_t *idx,
                       const unsigned int m_start, const unsigned int m_end,
                       const unsigned int K, const unsigned int N,
                       const unsigned int P_W,
                       const unsigned int p_start, const unsigned int p_end) {

  for (unsigned int m = m_start; m < m_end; m += 4) {
    unsigned int p = p_start;

    while (p < p_end) {
      size_t gvl;
      asm volatile("vsetvli %0, %1, e" _XSTR(DATA_TYPE_WIDTH) ", m4, ta, ma"
                   : "=r"(gvl) : "r"(p_end - p));

      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v4, 0");
      asm volatile("vmv.v.i v8, 0");
      asm volatile("vmv.v.i v12, 0");

      const spmm_data_t *w_col = w + p;
      const uint32_t *idx_col = idx + p;
      for (unsigned int k = 0; k < K; k++) {
        const spmm_data_t *a0 = &a[(m + 0) * K + k];
        const spmm_data_t *a1 = &a[(m + 1) * K + k];
        const spmm_data_t *a2 = &a[(m + 2) * K + k];
        const spmm_data_t *a3 = &a[(m + 3) * K + k];

        // Load indices (zero latency)
        ASM_VLX32_V20(idx_col);

        // Load compact weights
        asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v16, (%0)" ::"r"(w_col) : "memory");

        // Load scalars and 4x sparse FMA
        asm volatile("fld fa0, (%0)" ::"r"(a0));
        ASM_VFXMACC_V0_FA0;
        asm volatile("fld fa1, (%0)" ::"r"(a1));
        ASM_VFXMACC_V4_FA1;
        asm volatile("fld fa2, (%0)" ::"r"(a2));
        ASM_VFXMACC_V8_FA2;
        asm volatile("fld fa3, (%0)" ::"r"(a3));
        ASM_VFXMACC_V12_FA3;

        w_col += P_W;
        idx_col += P_W;
      }

      asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v0, (%0)"  ::"r"(c + (m + 0) * N + p) : "memory");
      asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v4, (%0)"  ::"r"(c + (m + 1) * N + p) : "memory");
      asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v8, (%0)"  ::"r"(c + (m + 2) * N + p) : "memory");
      asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v12, (%0)" ::"r"(c + (m + 3) * N + p) : "memory");
      p += gvl;
    }
  }
}

#endif // SP_FSPMM_H
