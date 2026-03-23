// Copyright 2025 ETH Zurich and University of Bologna.
// SPDX-License-Identifier: Apache-2.0
// Author: Bowen Wang, ETH Zurich
//
// Dense GEMM kernel with 4-row unrolling.
// Supports fp16 and fp32 via DATA_TYPE_WIDTH macro.
// Per K iteration: 1 vle (B row) + 4 vfmacc (one per row).

#ifndef SP_FMATMUL_H
#define SP_FMATMUL_H

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

// Data type alias
#if DATA_TYPE_WIDTH == 16
typedef uint16_t gemm_data_t;
#elif DATA_TYPE_WIDTH == 32
typedef float gemm_data_t;
#endif

inline void matmul_4xVL(gemm_data_t *c, const gemm_data_t *a, const gemm_data_t *b,
                         const unsigned int m_start, const unsigned int m_end,
                         const unsigned int K, const unsigned int N,
                         const unsigned int p_start, const unsigned int p_end)
    __attribute__((always_inline));

inline void matmul_4xVL(gemm_data_t *c, const gemm_data_t *a, const gemm_data_t *b,
                         const unsigned int m_start, const unsigned int m_end,
                         const unsigned int K, const unsigned int N,
                         const unsigned int p_start, const unsigned int p_end) {

  for (unsigned int m = m_start; m < m_end; m += 4) {
    unsigned int p = p_start;

    while (p < p_end) {
      size_t gvl;
      asm volatile("vsetvli %0, %1, e" _XSTR(DATA_TYPE_WIDTH) ", m4, ta, ma"
                   : "=r"(gvl) : "r"(p_end - p));

      // Zero 4 accumulators
      asm volatile("vmv.v.i v0, 0");
      asm volatile("vmv.v.i v4, 0");
      asm volatile("vmv.v.i v8, 0");
      asm volatile("vmv.v.i v12, 0");

      const gemm_data_t *b_col = b + p;
      for (unsigned int k = 0; k < K; k++) {
        // Load scalar from each of 4 rows of A into FP register via address
        const gemm_data_t *a0 = &a[(m + 0) * K + k];
        const gemm_data_t *a1 = &a[(m + 1) * K + k];
        const gemm_data_t *a2 = &a[(m + 2) * K + k];
        const gemm_data_t *a3 = &a[(m + 3) * K + k];

        // Load B row
        asm volatile("vle" _XSTR(DATA_TYPE_WIDTH) ".v v16, (%0)" ::"r"(b_col) : "memory");

        // Load scalars and FMA
        asm volatile("fld fa0, (%0)" ::"r"(a0));
        asm volatile("vfmacc.vf v0, fa0, v16");
        asm volatile("fld fa1, (%0)" ::"r"(a1));
        asm volatile("vfmacc.vf v4, fa1, v16");
        asm volatile("fld fa2, (%0)" ::"r"(a2));
        asm volatile("vfmacc.vf v8, fa2, v16");
        asm volatile("fld fa3, (%0)" ::"r"(a3));
        asm volatile("vfmacc.vf v12, fa3, v16");

        b_col += N;
      }

      // Store 4 result rows
      asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v0, (%0)"  ::"r"(c + (m + 0) * N + p) : "memory");
      asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v4, (%0)"  ::"r"(c + (m + 1) * N + p) : "memory");
      asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v8, (%0)"  ::"r"(c + (m + 2) * N + p) : "memory");
      asm volatile("vse" _XSTR(DATA_TYPE_WIDTH) ".v v12, (%0)" ::"r"(c + (m + 3) * N + p) : "memory");
      p += gvl;
    }
  }
}

#endif // SP_FMATMUL_H
