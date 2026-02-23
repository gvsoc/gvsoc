/* spatz_rvv_extensions.h — C-safe helpers for custom OPVFX encodings
 *
 * Layout (per your spec):
 * [31:26] funct6 | [25] vm | [24:20] vs2 | [19:15] vs1 | [14:12] frs1(3b)
 * [11:7]  vd     | [6:0]  opcode(0x56)
 */

#ifndef SPATZ_RVV_EXTENSIONS_H
#define SPATZ_RVV_EXTENSIONS_H

#include <stdint.h>

/* ---------- Opcode / funct6 ---------- */
#define OPC_OPVFX           0x56u      /* [6:0] */
#define FUNCT6_VFWXMACC     0x31u      /* 110001b -> [31:26] */
#define FUNCT6_VFWXMUL      0x33u      /* 110011b -> [31:26] */

/* ---------- Register index constants (NUMBERS, not names) ---------- */
/* Vector regs v0..v31 */
#define RVV_V0   0u
#define RVV_V1   1u
#define RVV_V2   2u
#define RVV_V3   3u
#define RVV_V4   4u
#define RVV_V5   5u
#define RVV_V6   6u
#define RVV_V7   7u
#define RVV_V8   8u
#define RVV_V9   9u
#define RVV_V10 10u
#define RVV_V11 11u
#define RVV_V12 12u
#define RVV_V13 13u
#define RVV_V14 14u
#define RVV_V15 15u
#define RVV_V16 16u
#define RVV_V17 17u
#define RVV_V18 18u
#define RVV_V19 19u
#define RVV_V20 20u
#define RVV_V21 21u
#define RVV_V22 22u
#define RVV_V23 23u
#define RVV_V24 24u
#define RVV_V25 25u
#define RVV_V26 26u
#define RVV_V27 27u
#define RVV_V28 28u
#define RVV_V29 29u
#define RVV_V30 30u
#define RVV_V31 31u

/* frs1 (3 bits) — ft0..ft7 only */
#define RVF_RS1_FT0 0u
#define RVF_RS1_FT1 1u
#define RVF_RS1_FT2 2u
#define RVF_RS1_FT3 3u
#define RVF_RS1_FT4 4u
#define RVF_RS1_FT5 5u
#define RVF_RS1_FT6 6u
#define RVF_RS1_FT7 7u

/* ---------- Core encoder (macro => compile-time const when given const args) ---------- */
#define ENCODE_OPVFX(funct6, vm, vs2, vs1, frs1_3b, vd) (        \
    ((uint32_t)((funct6)   & 0x3Fu) << 26) |                      \
    ((uint32_t)((vm)       & 0x01u) << 25) |                      \
    ((uint32_t)((vs2)      & 0x1Fu) << 20) |                      \
    ((uint32_t)((vs1)      & 0x1Fu) << 15) |                      \
    ((uint32_t)((frs1_3b)  & 0x07u) << 12) |                      \
    ((uint32_t)((vd)       & 0x1Fu) <<  7) |                      \
    ((uint32_t)OPC_OPVFX) )

/* ---------- Per-instruction wrappers ---------- */
#define VFWXMACC_VF(vd, vs2, vs1, frs1_3b, vm) \
    ENCODE_OPVFX(FUNCT6_VFWXMACC, (vm), (vs2), (vs1), (frs1_3b), (vd))

#define VFWXMUL_VF(vd, vs2, vs1, frs1_3b, vm) \
    ENCODE_OPVFX(FUNCT6_VFWXMUL,  (vm), (vs2), (vs1), (frs1_3b), (vd))

/* ---------- Emission helpers ---------- */
/* Use when all args are compile-time constants: puts the word directly. */
#define EMIT_WORD_IMM(word32_const) \
    asm volatile(".word %0" :: "i"( (uint32_t)(word32_const) ))

/* Use when any arg is runtime/computed: loads into a temp reg then emits. */
#define EMIT_WORD_REG(word32_val) do {      \
    uint32_t __w = (uint32_t)(word32_val);  \
    asm volatile(".word %0" :: "r"(__w));   \
} while (0)

/* Optional compile-time range checks when using C11+ */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define SPATZ_STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)
SPATZ_STATIC_ASSERT(OPC_OPVFX <= 0x7F, "opcode must fit in 7 bits");
#endif

#endif /* SPATZ_RVV_EXTENSIONS_H */
