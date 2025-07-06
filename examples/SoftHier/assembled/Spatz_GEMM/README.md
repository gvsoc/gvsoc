# MatMul Implementation with Spatz 
This application implemented the vectorized MatMul kernel (MxNxP) in SoftHier with Spatz vector engine. To use this application, you can first generate the data header file:
```
python {SOFTHIER_ROOT}/examples/SoftHier/assembled/Spatz_GEMM/software/util/spatz_matmul_datagen.py -M 16 -N 16 -P 16
```
Then build hardware and software with the provided configuration file:
```
cfg={SOFTHIER_ROOT}/examples/SoftHier/assembled/Spatz_GEMM/config/arch_spatz.py app=examples/SoftHier/assembled/Spatz_GEMM/software make hs; make run
```

## Updates

### Merged dense/sparse data generation script
How to use:
```tcl
# for dense 
python examples/SoftHier/assembled/Spatz_GEMM/software/util/spatz_matmul_datagen.py -M 32 -N 32 -P 32 -spN 2 -spM 4

# for sparse (index fp8)
python examples/SoftHier/assembled/Spatz_GEMM/software/util/spatz_matmul_datagen.py -M 32 -N 32 -P 32 -spN 2 -spM 4 --sparse

# for sparse (compact)
python examples/SoftHier/assembled/Spatz_GEMM/software/util/spatz_matmul_datagen.py -M 32 -N 32 -P 32 -spN 2 -spM 4 --sparse --idx_compact
```

### Add compact index tensor generator (25.06.25)
In this update, data generator script for compact N:M format has been added `write_index_to_header()`.
- Currently only support 2:4 format for verification
- Script utilization is the same
- [ ] Add all N:M format to the generation script
Added AspB MatMul kernel utilizing `vfwxmul.vf` and `vfwxmacc.vf` instructions, passed functional verification.
- [ ] Pending instruction timing verification.
To analyze performance with UI, use the following command:
```tcl
cfg=examples/SoftHier/assembled/Spatz_GEMM/config/arch_spatz.py app=examples/SoftHier/assembled/Spatz_GEMM/software make hs; make runv pfto
```
Trace file will be generated at `sw_build/perfetto.json`. Use this trace file to analyze instruction timing.

### Software emulation of AspB MatMul (19.06.25)
In this update, we assume matrix B is sparse with N:M format, and stored in a compact way along with the index matrix. Matrix A is still dense. The updated sw shows configuration and index translation overhead.

To run the simulation:
```tcl
# data generation
python examples/SoftHier/assembled/Spatz_GEMM/software/util/spatz_sp_matmul_datagen.py -M 16 -N 16 -P 16 -spN 2 -spM 4
# run simulation
cfg=examples/SoftHier/assembled/Spatz_GEMM/config/arch_spatz.py app=examples/SoftHier/assembled/Spatz_GEMM/software make hs; make run
```
#### Attention!
- We utilize `vsuxei8.v` instruction to handle index store operations. Therefore, the current implementation only support dimension `P` up to `256`. 
- `fp16` accumulation is by default.
- Index load and stroe is through VLSU. More efficiently is through VSLIDE unit. Could be extended baseline emulation.
- [TODO] Merge data generation scripts.


### Widening accumulation (18.06.25)
In this update, input element have `fp8` precision, and accumulate with `fp16` precision. 

## Note
- Current implementation is based on fp8 (E5M2) format without widening accumulation. Rounding implementations in GVSoC and `datagen` script is different (GVSoC: per instruction, python: per kernel), which contributes to the mismatch in verification.
