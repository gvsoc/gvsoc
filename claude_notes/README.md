# Ventaglio Spatz — Implementation Notes

## Branch: `soft_hier_vtl` (based on `soft_hier_je`)

---
---

# Part 5: E2E Dense vs SpMM Comparison (SUMMA, 4-row unrolled tiles)

## CHANGES

### `implementation/sw/Ventaglio_Spatz_GEMM/include/spatz_tile_gemm.h`
### `implementation/sw/Ventaglio_Spatz_SpMM/include/spatz_tile_spmm.h`
- **4-row unrolled**: Per K iteration loads W once, then does 4 FMAs with different X scalars
- This amortizes scalar overhead (fld, address math) over 4 FMAs, making FMA the bottleneck
- Without this, the SUMMA tile compute was single-row and scalar-bound (~1.1x speedup)

### `implementation/sw/Ventaglio_Spatz_SpMM/` (pipeline kernel)
- SUMMA SpMM kernel using `SummaSpatzSpMM.h` with sparse tile compute
- `spatz_spmm()` engine method, `'spatz_spmm'` flow dispatch, `SpatzSpMM` config class

### `implementation/experiments/Trash/qwen_on_softhier.py`
- Runs BOTH dense GEMM and SpMM 2:4 back-to-back for direct comparison
- Prints per-kernel speedup table

## E2E RESULTS (Qwen-Medium fp16: embed=1024, heads=8, mlp=2048, batch=4, spec=4)

```
Kernel               |    Dense |  SpMM 2:4 | Speedup
---------------------------------------------------------
attn_q_proj          |  76.8 us |  46.1 us  |  1.67x
attn_k_proj          |  38.8 us |  23.3 us  |  1.66x
attn_v_proj          |  38.8 us |  23.3 us  |  1.66x
attn_o_proj          |  76.9 us |  46.1 us  |  1.67x
ffn_up_proj          | 146.0 us |  84.2 us  |  1.73x
ffn_gate_proj        | 146.0 us |  84.2 us  |  1.73x
ffn_down_proj        | 148.3 us |  86.5 us  |  1.71x
Non-GEMM (same)      |   8.3 us |   8.3 us  |  1.00x
---------------------------------------------------------
Layer TOTAL          | 680.5 us | 402.6 us  |  1.69x
```

- **GEMM speedup: 1.66-1.73×** (larger N → closer to 2× ideal)
- **Layer-level speedup: 1.69× (2:4), 1.74× (1:4)** including non-GEMM overhead
- FFN GEMMs (N=2048) outperform attention GEMMs (N=1024) as expected

### 3-Way Comparison (Dense vs 2:4 vs 1:4)

```
Kernel               |     Dense |       2:4 |       1:4 | 2:4 spd | 1:4 spd
---------------------------------------------------------------------------
attn_q_proj          |  76.8 us |  46.1 us |  43.9 us |  1.67x |  1.75x
attn_k_proj          |  38.8 us |  23.3 us |  22.2 us |  1.66x |  1.75x
attn_o_proj          |  76.9 us |  46.1 us |  43.9 us |  1.67x |  1.75x
ffn_up_proj          | 146.0 us |  84.2 us |  82.8 us |  1.73x |  1.76x
ffn_gate_proj        | 146.0 us |  84.2 us |  82.8 us |  1.73x |  1.76x
ffn_down_proj        | 148.3 us |  86.5 us |  84.2 us |  1.71x |  1.76x
---------------------------------------------------------------------------
TOTAL                | 680.5 us | 402.6 us | 390.8 us |  1.69x |  1.74x
```

**Finding**: 1:4 was only marginally faster than 2:4 with RedMule-sized tiles (N_tile=128).
Fixed with Spatz-specific optimizer (`spatz_gemm_auto.py`) using N_tile=1024.

### Final Results with Spatz Optimizer (N_tile=1024, 4-row unrolled tiles)

```
Kernel               |    Dense |     2:4 |     1:4 | 2:4 spd | 1:4 spd
------------------------------------------------------------------------
attn_q_proj          |  77.6 us | 44.2 us | 27.6 us |  1.75x |  2.81x
attn_k_proj          |  39.7 us | 23.1 us | 14.8 us |  1.72x |  2.69x
attn_o_proj          |  77.6 us | 44.2 us | 27.6 us |  1.76x |  2.81x
ffn_up_proj          | 152.1 us | 86.0 us | 52.9 us |  1.77x |  2.87x
ffn_gate_proj        | 152.1 us | 86.0 us | 52.9 us |  1.77x |  2.88x
ffn_down_proj        | 151.9 us | 85.8 us | 52.7 us |  1.77x |  2.88x
------------------------------------------------------------------------
Layer TOTAL          | 699.8 us | 401.2 us | 252.3 us | 1.74x |  2.77x
```

### Gap to Theoretical Limit (2× for 2:4, 4× for 1:4)

Two sources of remaining overhead:

1. **Fixed scalar overhead** per K iteration (~5 cycles for 4×fld + loop control)
   that doesn't reduce with sparsity. Already amortized by 4-row unrolling but not zero.

2. **SUMMA DMA loads full N_tile** regardless of sparsity — DMA time is the same for
   dense and sparse. Only compute benefits from sparsity. Sparse-aware DMA (loading
   PW_tile instead of N_tile) would reduce this overhead proportionally.

### Final Results: Real Qwen-7B with Sparse-Aware DMA

**Model**: Qwen-7B-Chat (embed=4096, heads=32, head_dim=128, GQA groups=8, MLP=22016)
**Workload**: Decode, speculative (factor=4), batch=16, KV cache=1020
**Sequence**: batch×spec = 64 tokens per layer
**Platform**: SoftHier 4×4 cluster grid, fp16, SUMMA dataflow

```
================================================================================
  Qwen-7B fp16: Dense vs SpMM 2:4 vs SpMM 1:4 (sparse-aware SUMMA DMA)
================================================================================
Kernel               |     Dense |       2:4 |       1:4 | 2:4 spd | 1:4 spd
--------------------------------------------------------------------------------
attn_q_proj          |  4369.7 us |  2233.4 us |  1165.7 us |   1.96x |   3.75x
attn_k_proj          |  1096.5 us |   560.7 us |   293.0 us |   1.96x |   3.74x
attn_v_proj          |  1096.5 us |   560.7 us |   293.0 us |   1.96x |   3.74x
attn_o_proj          |  4369.8 us |  2233.4 us |  1165.6 us |   1.96x |   3.75x
ffn_up_proj          | 24039.9 us | 12289.1 us |  6411.4 us |   1.96x |   3.75x
ffn_gate_proj        | 24040.7 us | 12289.1 us |  6411.5 us |   1.96x |   3.75x
ffn_down_proj        | 23433.5 us | 11976.0 us |  6248.1 us |   1.96x |   3.75x
attn_mha             |   378.7 us |   378.7 us |   378.7 us |   1.00x |   1.00x
Non-GEMM (norm,rope, |    68.7 us |    68.7 us |    68.7 us |   1.00x |   1.00x
  acti,resnet)       |           |           |           |         |
--------------------------------------------------------------------------------
TOTAL (1 layer)      | 82893.8 us | 42589.7 us | 22435.6 us |   1.95x |   3.69x
```

**Near-theoretical speedups on real model dimensions:**
- **2:4 SpMM: 1.96× per GEMM** (theoretical: 2.0×)
- **1:4 SpMM: 3.75× per GEMM** (theoretical: 4.0×)
- **Layer total: 1.95× (2:4), 3.69× (1:4)** — gap from theoretical due to
  fixed non-GEMM overhead (attention=378µs, norm/rope/acti=69µs)

### Runtime Decomposition & DMA Analysis

Using 3-point analysis (Dense, 2:4, 1:4), we solve for the scalable vs fixed portions:
`T = T_scalable × (1/sparsity_ratio) + T_fixed`

```
Kernel               |    Dense |      2:4 |      1:4 |  Scalable |   Fixed | Eff.
----------------------------------------------------------------------------------
attn_q_proj          |  4370 us |  2233 us |  1166 us |  4273 us |   97 us |  98%
attn_k/v_proj (each) |  1096 us |   561 us |   293 us |  1072 us |   25 us |  98%
attn_o_proj          |  4370 us |  2233 us |  1166 us |  4273 us |   97 us |  98%
ffn_up/gate (each)   | 24040 us | 12289 us |  6411 us | 23502 us |  538 us |  98%
ffn_down_proj        | 23433 us | 11976 us |  6248 us | 22915 us |  519 us |  98%
----------------------------------------------------------------------------------
GEMM Total           | 82446 us | 42142 us | 21988 us | 80608 us | 1838 us |  98%
```

**97.8% of runtime is scalable** (DMA + compute), only 2.2% is fixed overhead.
This explains the near-theoretical speedups: 1.96x (2:4) and 3.75x (1:4).

### Weight DMA Dominance (Decode Mode)

In decode mode (M=64 tokens), weight DMA dominates data transactions:

| | Dense | 2:4 SpMM | 1:4 SpMM |
|---|---|---|---|
| **Weight DMA/layer** | **596 MB** | **298 MB (-50%)** | **149 MB (-75%)** |
| Activation DMA/layer | 5.7 MB | 5.7 MB | 5.7 MB |
| **Full model (32 layers)** | **18.6 GB** | **9.3 GB** | **4.7 GB** |

Per-tile: W_tile = 89% of tile DMA (64KB W vs 8KB X per SUMMA K iteration).
Sparse-aware DMA reduces W transfers proportionally → both DMA and compute
scale together → near-ideal speedup.

### Batch=1 Decode (GEMV/SpMV): M=4, speculative

```
=====================================================================================
  Qwen-7B fp16 DECODE batch=1 (M=4): Dense GEMV vs SpMV 2:4 vs SpMV 1:4
=====================================================================================
Kernel               |     Dense |       2:4 |       1:4 | 2:4 spd | 1:4 spd
-------------------------------------------------------------------------------------
attn_q_proj          |   280.9 us |   143.5 us |    74.7 us |   1.96x |   3.76x
attn_k/v_proj (each) |    77.8 us |    40.1 us |    21.2 us |   1.94x |   3.66x
attn_o_proj          |   280.8 us |   143.5 us |    74.7 us |   1.96x |   3.76x
ffn_up_proj          |  1601.1 us |   826.0 us |   434.2 us |   1.94x |   3.69x
ffn_gate_proj        |  1601.1 us |   826.0 us |   434.1 us |   1.94x |   3.69x
ffn_down_proj        |  1467.9 us |   748.3 us |   388.1 us |   1.96x |   3.78x
-------------------------------------------------------------------------------------
Layer TOTAL          |  5428.7 us |  2808.8 us |  1489.7 us |   1.93x |   3.64x
```

Sparsity speedups consistent across batch sizes (batch=1 vs batch=16).
GEMV/SpMV tile kernel automatically selected when M_tile <= 1.

### Implementation: GEMV/SpMV Tile Kernels

- `spatz_tile_gemv.h`: Single-row dense GEMV (no M-unrolling, K-loop only)
- `spatz_tile_spmv.h`: Single-row sparse SpMV with vlx + vfxmacc
- `SummaSpatzGEMM.h` / `SummaSpatzSpMM.h`: Auto-select GEMV/SpMV when M_tile <= 1
- `qwen_on_softhier.py`: RoPE contiguous_length fix for small M (batch=1)

Key enablers for near-theoretical speedup:
1. **Sparse-aware SUMMA DMA** — W tile loads use PW_tile (compact) width, reducing
   memory traffic proportionally to sparsity ratio
2. **L1-constrained optimizer** — tiles sized to fit in TCDM with double-buffering
3. **4-row unrolled tile compute** — amortizes scalar overhead over 4 FMAs per load
4. **Real model dimensions** (N=4096, 22016) — FMA completely dominates scalar overhead

### Key Implementation: Spatz Optimizer (`spatz_gemm_auto.py`)

The RedMule optimizer (`gemm_auto.py`) uses small tiles based on `4 × redmule_ce_height = 512`
and aggressively splits N across clusters. This gives N_tile=128 which is too small for
fp16's 16-wide FPU — FMA latency hits the floor.

The Spatz optimizer uses target N_tile=1024, prefers split-K over split-N to preserve
large N_tile, ensuring FMA dominates over scalar overhead for all sparsity ratios.

## HOW TO RUN

```bash
cd /scratch2/bowwang/claude_space/gvsoc
source sourceme.sh
cd implementation
test=experiments/Trash/qwen_on_softhier.py make llm
```

The script automatically runs Dense GEMM then SpMM 2:4 and prints the comparison.

---
---

# Part 4: fp16 E2E Pipeline Integration

## CHANGES

### `implementation/experiments/Trash/qwen_on_softhier.py`
- Model dtype changed from fp32 to **fp16** — all kernels now use consistent precision
- GEMM kernels: `'gemm'` (RedMule) → `'spatz_gemm'` (Spatz fp16) via `SpatzGEMM` config
- Small "Qwen-Tiny" model for fast verification: embed=256, heads=4, mlp=512

### `implementation/sw/Ventaglio_Spatz_GEMM/include/spatz_tile_gemm.h`
- Parameterized for fp16/fp32 via `DATA_TYPE_WIDTH` macro
- Uses `vle16.v`/`vse16.v` for fp16, `vle32.v`/`vse32.v` for fp32
- Scalar loads via `fld` from address (works for both precisions)

### `implementation/sw/Ventaglio_Spatz_GEMM/include/sp_fmatmul.h` (standalone kernel)
- Parameterized: `gemm_data_t` = `uint16_t` (fp16) or `float` (fp32)
- 4-row unrolled with `_XSTR(DATA_TYPE_WIDTH)` for instruction suffixes

### `examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/software/util/datagen.py`
- Added `--dtype fp16` flag for fp16 data generation

### `examples/SoftHier/assembled/Ventaglio_Spatz_SpMM/` (all files)
- Same fp16/fp32 parameterization applied to SpMM kernel and datagen

### `implementation/scripts/utils/softhier_engine.py`
- `spatz_gemm()`: config header now dtype-aware (DATA_TYPE_WIDTH = 16 or 32)

### `implementation/scripts/utils/analyze_results.py`
- Fixed `KeyError: 'redmule_uti'` — use `.get()` with default 0 for Spatz GEMM results

## E2E RESULTS (Qwen-Tiny fp16, decode, batch=4, speculative=4)

```
Kernel         | Runtime | Share
---------------|---------|------
attn_q_proj    |  7.1 µs |  11%
attn_k_proj    |  4.5 µs |   7%
attn_v_proj    |  4.5 µs |   7%
attn_o_proj    |  7.1 µs |  11%
ffn_up_proj    | 12.8 µs |  19%
ffn_gate_proj  | 12.8 µs |  19%
ffn_down_proj  | 12.7 µs |  19%
Other          |  6.5 µs |  10%
TOTAL          | 66.7 µs | 100%
```

GEMM kernels (Spatz fp16) account for ~93% of total layer runtime.

## HOW TO RUN

```bash
cd /scratch2/bowwang/claude_space/gvsoc
source sourceme.sh
cd implementation
test=experiments/Trash/qwen_on_softhier.py make llm
```

---
---

# Part 3: Ventaglio N:M Sparse Instructions + SpMM Kernel

## CREATED

### GVSoC ISS — Ventaglio instruction support
- `vlx32.v` — N:M index load (zero latency, pipelined with Ventaglio gather)
- `vfxmacc.vf` — Sparse FMA (same per-element latency as dense; speedup from compact VL)
- `vfxmul.vf` — Sparse multiply

### `examples/SoftHier/assembled/Ventaglio_Spatz_SpMM/`
N:M sparse matrix multiplication kernel using Ventaglio custom instructions.

```
Ventaglio_Spatz_SpMM/
├── config/arch_spatz.py
└── software/
    ├── CMakeLists.txt
    ├── main.c                     # SoftHier test harness
    ├── include/
    │   ├── sp_fspmm.h             # 4-row unrolled SpMM kernel (vlx32 + vfxmacc)
    │   └── data_spmm_fp32.h       # Auto-generated sparse test data
    └── util/
        └── datagen.py             # Generates N:M sparse data (--n_sparse, --m_sparse)
```

## CHANGES

### `core/models/cpu/iss/isa_gen/isa_rvv.py`
- Added `vfxmacc.vf` (funct6=100101), `vfxmul.vf` (funct6=100110), `vlx32.v` instruction entries

### `core/models/cpu/iss/include/isa/rv32v.hpp`
- Added `vlx32_v_exec`, `vfxmacc_vf_exec`, `vfxmul_vf_exec` execution functions
- **Vector chaining**: All `vle*.v` loads use 1-cycle latency (chained with FPU)
- **Decoupled execution**: `vfmacc.vf` and `vfxmacc.vf` update scoreboard but do NOT stall the scalar core — scalar instructions (flw, addi, bne) run freely during vector FMA, matching the Snitch dual-issue hardware model
- **Zero-latency vlx**: Index loads are fully pipelined (no stall, no scoreboard entry)

### `core/models/cpu/iss/include/spatz.hpp`
- Added `idx_regs[]` index buffer (separate from vregs, for Ventaglio indices)
- Added `sparse_n`, `sparse_m` fields for sparsity configuration

### `core/models/cpu/iss/include/isa_lib/vint.h`
- Added `lib_VLX32V` (loads to idx_regs), `lib_FXMACCVF`, `lib_FXMULVF`

### `core/models/cpu/iss/src/spatz.cpp`
- Initialize `sparse_n=2`, `sparse_m=4` and zero `idx_regs` on reset

## TIMING MODEL

The speedup from N:M sparsity comes from **compact storage** (smaller VL), not reduced per-element FMA cost:

```
Dense GEMM:   VL = N     → FMA latency = (N + FPU-1) / FPU
SpMM 2:4:     VL = N/2   → FMA latency = (N/2 + FPU-1) / FPU  → ~2x faster
SpMM 1:4:     VL = N/4   → FMA latency = (N/4 + FPU-1) / FPU  → ~4x faster
```

Additional modeling:
- **Vector chaining**: vle/vlx latency = 1 cycle (load chained with downstream FPU)
- **Decoupled execution**: Scalar core runs ahead during vector FMA (no stall on vfmacc/vfxmacc)
- **Zero-cost index load**: vlx32.v has zero latency (pipelined with Ventaglio gather unit)

## BENCHMARK RESULTS

4-row unrolled kernels, single cluster (core 0 with Spatz):

| Size (MxKxN) | Dense (ns) | 2:4 (ns) | 1:4 (ns) | 2:4 speedup | 1:4 speedup |
|---|---|---|---|---|---|
| 4×16×16 | 211 | 220 | 220 | 0.96× | 0.96× |
| 4×32×32 | 581 | 482 | 480 | 1.21× | 1.21× |
| 8×32×64 | 2,199 | 1,173 | 1,043 | 1.87× | 2.11× |
| 8×64×64 | 4,307 | 2,258 | 2,000 | 1.91× | 2.15× |
| 16×64×64 | 8,603 | 4,506 | 3,739 | 1.91× | 2.30× |
| 16×64×128 | 16,816 | 8,619 | 4,510 | 1.95× | **3.73×** |
| 16×128×64 | 17,056 | 8,859 | 7,324 | 1.93× | 2.33× |

- 2:4 converges to **~1.9×** (ideal: 2.0×) at scale
- 1:4 reaches **up to 3.73×** (ideal: 4.0×) for large N
- Small sizes show no benefit (scalar overhead dominates)

## HOW TO USE

```bash
cd /scratch2/bowwang/claude_space/gvsoc
source sourceme.sh

# Generate sparse data (configurable sparsity)
python3 examples/SoftHier/assembled/Ventaglio_Spatz_SpMM/software/util/datagen.py \
    -M 16 -K 64 -N 128 --n_sparse 2 --m_sparse 4   # 2:4 format
python3 examples/SoftHier/assembled/Ventaglio_Spatz_SpMM/software/util/datagen.py \
    -M 16 -K 64 -N 128 --n_sparse 1 --m_sparse 4   # 1:4 format

# Build and run
rm -rf sw_build
cfg=examples/SoftHier/assembled/Ventaglio_Spatz_SpMM/config/arch_spatz.py \
app=examples/SoftHier/assembled/Ventaglio_Spatz_SpMM/software \
make hs && make run
```

### Rebuilding GVSoC after ISS changes
```bash
touch core/models/cpu/iss/include/isa/rv32v.hpp
cd build && make -j6 && make install && cd ..
```

---
---

# Part 2: End-to-End Pipeline Integration (SUMMA + Spatz GEMM)

## CREATED

### `implementation/sw/Ventaglio_Spatz_GEMM/`
SUMMA GEMM kernel using Spatz vector processor for local tile computation. Drop-in replacement for `SummaGEMM/` (RedMule) in the E2E pipeline.

```
Ventaglio_Spatz_GEMM/
├── CMakeLists.txt
├── main.c                            # SUMMA entry point (same structure as SummaGEMM/main.c)
└── include/
    ├── SummaSpatzGEMM.h              # Full SUMMA dataflow with Spatz local compute
    ├── spatz_tile_gemm.h             # Accumulating tile GEMM: Z += X * W via Spatz RVV
    ├── spatz_gemm.h                  # Generated config header (M/N/K, tiles, SUMMA params)
    └── preload.h                     # Generated HBM addresses
```

### `implementation/config/kernels/spatz_gemm.py`
`SpatzGEMM` config class — compatible with the existing `gemm_auto` optimizer for SUMMA tiling.

## CHANGES

### `implementation/scripts/utils/softhier_engine.py`
- Added `spatz_gemm()` method that uses the same `gemm_optimizer.opt()` as RedMule GEMM for SUMMA tiling, then generates Spatz-specific config headers and compiles `sw/Ventaglio_Spatz_GEMM/`.

### `implementation/scripts/llm/flow.py`
- Added `'spatz_gemm'` kernel type dispatch in `softhier_run_flow()` → calls `chip.spatz_gemm()`.

### `implementation/scripts/utils/kernel_configuration.py`
- Added `fp32` dtype handling in `generate_config_C_header()` (DATA_TYPE_WIDTH=32, DATA_TYPE_BYTE=4).

### `implementation/scripts/llm/normal_llm_plan.py`
- Fixed `elem_size` to handle `fp32` (4 bytes) in all three plan functions.

### `implementation/Makefile`
- Added `SPATZ_GEMM_FILE` config path to the `llm` target.

### `implementation/experiments/Trash/qwen_on_softhier.py`
- Changed model dtype to `fp32`.
- After layer planning, converts all `'gemm'` kernels to `'spatz_gemm'` with `SpatzGEMM` configs.
- Non-GEMM kernels kept at `fp16` for SW compatibility (their C code doesn't support fp32 yet).
- Full simulation commented out; dry run only for now.

## ARCHITECTURE

The SUMMA dataflow is **identical** to the original RedMule version. Only the local tile compute is replaced:

```
Original (RedMule):                    New (Spatz):
┌─────────────────────┐              ┌─────────────────────┐
│  SUMMA Dataflow     │              │  SUMMA Dataflow     │
│  - Tiling/Optimizer │  SAME        │  - Tiling/Optimizer │
│  - DMA prefetch     │ ═══════════> │  - DMA prefetch     │
│  - Broadcast        │              │  - Broadcast        │
│  - Double-buffer    │              │  - Double-buffer    │
│  - Group barriers   │              │  - Group barriers   │
│  - Reduction        │              │  - Reduction        │
├─────────────────────┤              ├─────────────────────┤
│  Local Tile Compute │  REPLACED    │  Local Tile Compute │
│  flex_redmule_*()   │ ═══════════> │  spatz_tile_gemm_   │
│  (async systolic)   │              │    fp32() (sync RVV)│
└─────────────────────┘              └─────────────────────┘
```

## HOW TO USE

### Dry run (compilation check only)
```bash
cd /scratch2/bowwang/claude_space/gvsoc
source sourceme.sh
cd implementation
test=experiments/Trash/qwen_on_softhier.py make llm
```

All 15 kernels of Qwen-7B decode layer compile successfully:
- 7 × `spatz_gemm` (Q/K/V/O projections, up/gate/down FFN)
- 2 × `norm`, 2 × `rope`, 1 × `flat_attn`, 1 × `acti`, 2 × `addi`

### Next steps
- Enable full simulation (uncomment in `qwen_on_softhier.py`)
- Implement fp16 Spatz GEMM to align with other kernels
- Port remaining kernels (norm, rope, acti) to fp32 if needed

---
---

# Part 1: Standalone Spatz GEMM Kernel

---

## CREATED

### `examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/`
Single-precision (fp32) GEMM kernel running on the Spatz vector processor within SoftHier.
Ported from the Spatz RTL benchmark (`spatz/sw/spatzBenchmarks/sp-fmatmul/kernel/sp-fmatmul.c`).

```
Ventaglio_Spatz_GEMM/
├── config/
│   └── arch_spatz.py                 # HW config (4x4 clusters, Spatz on core 0)
├── preload/
│   └── preload.elf                   # Generated HBM preload (for large matrices)
└── software/
    ├── CMakeLists.txt
    ├── main.c                        # SoftHier entry point (supports embedded + HBM modes)
    ├── include/
    │   ├── sp_fmatmul.h              # fp32 GEMM kernel using RVV vfmacc.vf
    │   └── data_fp32.h               # Auto-generated (by either datagen script)
    └── util/
        ├── datagen.py                # Embedded mode: generates static data header (<64KB)
        └── hbm_datagen.py            # HBM mode: generates preload ELF (any size)
```

---

## CHANGES

None to existing files. All work is new files under `examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/`.

---

## KEY FINDINGS

### 1. `"memory"` clobber required on vector load/store inline asm
Without `: "memory"` on `vle32.v` / `vse32.v` asm statements, GCC may reorder scalar stores past vector loads, causing stale data reads. This is a **compiler issue**, not an ISS bug.

### 2. M-outer loop order required
The kernel only produces correct results with the M (row) loop as the outermost loop. P-outer (column-tiled) loop ordering fails due to GCC optimizer interactions with the ISS. The original Spatz `matmul_4xVL` double-buffered kernel is not directly portable; a simpler single-accumulator structure is used instead.

### 3. `flex_alloc_init()` overwrites HBM preload data
The HBM heap allocator writes metadata to `0xc0000000`, destroying any preloaded data at offset 0. In HBM preload mode, `flex_alloc_init()` must be skipped.

---

## HOW TO USE

### Embedded mode (small matrices, data baked into binary)

```bash
cd /scratch2/bowwang/claude_space/gvsoc
source sourceme.sh

# Generate test data (fits in 64KB instruction memory)
python3 examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/software/util/datagen.py \
    -M 16 -K 16 -N 16

# Build and run
cfg=examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/config/arch_spatz.py \
app=examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/software \
make hs && make run
```

### HBM preload mode (large matrices)

```bash
# Generate HBM preload ELF
python3 examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/software/util/hbm_datagen.py \
    -M 32 -K 128 -N 128

# Clean build (required when switching between modes)
rm -rf sw_build

# Build
cfg=examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/config/arch_spatz.py \
app=examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/software \
make hs

# Run with preload
pld=examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/preload/preload.elf make run
```

### Performance trace

```bash
pld=examples/SoftHier/assembled/Ventaglio_Spatz_GEMM/preload/preload.elf make runv pfto
# Output: sw_build/perfetto.json (open in https://ui.perfetto.dev/)
```

### Tested configurations

| Size (MxKxN) | Mode     | Runtime    | Status |
|--------------|----------|------------|--------|
| 16x16x16     | Embedded | 1,784 ns   | PASS   |
| 32x64x32     | Embedded | 21,067 ns  | PASS   |
| 64x64x64     | Embedded | 70,730 ns  | PASS   |
| 32x128x128   | HBM      | 256,302 ns | PASS   |
