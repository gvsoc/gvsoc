# MatMul Implementation with Spatz 
This application implemented the vectorized MatMul kernel (MxNxP) in SoftHier with Spatz vector engine. To use this application, you can first generate the data header file:
```
python {SOFTHIER_ROOT}/examples/SoftHier/assembled/Spatz_GEMM/software/util/spatz_matmul_datagen.py -M 16 -N 16 -P 16
```
Then build hardware and software with the provided configuration file:
```
cfg={SOFTHIER_ROOT}/examples/SoftHier/assembled/Spatz_GEMM/config/arch_spatz.py app=examples/SoftHier/assembled/Spatz_GEMM/software make hs; make run
```

## Note
- Current implementation is based on fp8 (E5M2) format without widening accumulation. Rounding implementations in GVSoC and `datagen` script is different (GVSoC: per instruction, python: per kernel), which contributes to the mismatch in verification.
