# Sparse-DMA gather in GVSoC

The implementation extends `pulp/pulp/idma` with the indexed gather operation
from the sibling `spatz-sparse-dma` RTL repository. Enable it with
`SnitchDma(..., gather_enable=True)` and bind `o_INDEX()` to an independent
64-bit TCDM read port using absolute local-memory addresses.

`DMIDX rs1, imm5` captures the index-stream address and element width
(`imm5[1:0]`: 8, 16, 32, or 64 bits). `DMCPYI`/`DMCPY` configuration bit 2
selects gather. The normal source/destination, stride, repetition and row-size
registers describe:

```
source(i)      = source_base + unsigned(index[i]) * source_stride
destination(i) = destination_base + i * destination_stride
bytes(i)       = transfer_size
```

Source stride must be a nonzero power of two, and the index-stream base must
be 8-byte aligned. Configuration is captured with each descriptor. Packed
indices share a read word; a partial final word does not create extra rows.
Completion covers all rows and respects descriptor issue order. Zero-size or
zero-repetition gathers complete without memory traffic. Ordinary 1D/2D
transfers retain their existing interface; gather takes precedence if both
gather and 2D configuration bits are set.

The `spatz_sparse_dma` target recreates the experiment with two Spatz/Snitch
cores at 1 GHz, 128 KiB of TCDM (16 × 64-bit banks), the 512-bit DMA path,
a dedicated index port, and HBM2E-3600. It uses the legacy io-v1 interface
required by the requested iDMA model. The unchanged RTL benchmark ELF files
run directly on it. Support components model the RTL UART/EOC registers,
instruction prefetch policy, and the 64-byte DRAM bridge. The bridge consumes
fabric delay before sending timed requests, rather than adding queued bus
delay after the controller response.
HTIF polling is disabled on this board so successful termination is recorded
through the same end-of-computation register as the RTL testbench.

## Reproduce

Prerequisites are the completed local RTL and GVSoC/DRAMSys setup, recorded in
`../spatz-sparse-dma/build/reproduce_sparse_dma/` and
`build/reproduce_dramsys/`. The wrapper reuses their toolchains and puts caches,
temporary files, binaries and evidence under the two repositories.

From the `gvsoc` directory:

```bash
bash scripts/sparse_dma/step.sh prepare
bash scripts/sparse_dma/step.sh build-dram
bash scripts/sparse_dma/step.sh build
bash scripts/sparse_dma/step.sh test
bash scripts/sparse_dma/step.sh run gather
bash scripts/sparse_dma/step.sh run gather-opt
bash scripts/sparse_dma/step.sh run gather-hw
bash scripts/sparse_dma/step.sh build-validation
bash scripts/sparse_dma/step.sh validate rtl
bash scripts/sparse_dma/step.sh validate gvsoc
bash scripts/sparse_dma/step.sh analyze
```

Run RTL cases sequentially: its launcher uses shared trace filenames. GVSoC
uses separate run directories. `SPARSE_DMA_CONFIG` can select another shared
parameter JSON. `archive.py NAME NOTES` preserves an iteration before changing
parameters or models; names must be new, so existing evidence is not replaced.
For a new calibration sweep, use
`bash scripts/sparse_dma/step.sh calibrate --output NEW_DIRECTORY_NAME`.
It checks the current shared fabric delay, then 6, 4, 10 and 12 cycles, stopping
when all cases meet both timing limits. The final checked-in delay is 4 cycles.

The matching DRAMSys sources are copied from the RTL dependency into
`build/sparse_dma/dramsys-rtl`. The generated API adapter changes `add_dram`
metadata/address handling; controller timing, scheduling and address mapping
remain those of the RTL dependency.

The functional test covers unsigned 8/16/32/64-bit indices (including high
address bits), partial packed words, queued configuration snapshots, normal
1D/2D copies, a 4 KiB boundary, empty transfers, combined mode bits, delayed
responses, denied index requests and small queues that stall the frontend.

## Evidence and accuracy

The generated [report](../../build/sparse_dma/REPORT.md) contains actual results,
calibration changes, limits and links to the simulation logs. Measurements and
plots are saved as CSV/JSON and PNG/SVG/PDF in `build/sparse_dma/`.

Software cycles come from the unmodified benchmark's `mcycle` measurement.
The separate DMA window runs from the first gather issue through the final
completion, excluding the initial index preload. Accuracy is checked per case
as `abs(GVSoC / RTL - 1) <= 0.10`; no fitted correction is applied to reported
cycle counts. Read the additional workload table before extrapolating beyond
the original experiment. The report records sensitivity to DRAM refresh phase;
full boot timing and other controller states have not been calibrated.

New instruction-prefetch behavior is specific to this calibration board.
Generic loader preload and jump timing options keep their previous defaults.
The io-v1 cache also handles asynchronous denied refills, and the iDMA AXI
backend forwards a ready row on the previous row's acknowledgement edge.
