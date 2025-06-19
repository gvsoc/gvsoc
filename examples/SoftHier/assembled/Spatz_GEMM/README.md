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

### Software emulation of AspB MatMul (19.06.25)
In this update, we assume matrix B is sparse with N:M format, and stored in a compact way along with the index matrix. Matrix A is still dense. The updated sw shows configuration and index translation overhead.

To run the simulation:
```tcl
# data generation
python examples/SoftHier/assembled/Spatz_GEMM/software/util/spatz_sp_matmul_datagen.py -M 16 -N 16 -P 16 -spN 2 -spM 4
# run simulation
cfg=examples/SoftHier/assembled/Spatz_GEMM/config/arch_spatz.py app=examples/SoftHier/assembled/Spatz_GEMM/software make hs; make run
```
[Attention!] 
- We utilize `vsuxei8.v` instruction to handle index store operations. Therefore, the current implementation only support dimension `P` up to `256`. 
- `fp16` accumulation is by default.
- Index load and stroe is through VLSU. More efficiently is through VSLIDE unit. Could be extended baseline emulation.
- [TODO] Merge data generation scripts.
- [TODO] Add test for `vsuxeiX.v` instructions.


### Widening accumulation (18.06.25)
In this update, input element have `fp8` precision, and accumulate with `fp16` precision. 

## Note
- Current implementation is based on fp8 (E5M2) format without widening accumulation. Rounding implementations in GVSoC and `datagen` script is different (GVSoC: per instruction, python: per kernel), which contributes to the mismatch in verification.
