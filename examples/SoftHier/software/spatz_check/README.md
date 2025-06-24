# Gather-Mul-Scatter instructions
`vfwxmul.vf` and `vfwxmacc.vf` custom RVV instructions has been added to Spatz model. This folder contains basic functional verification for these instructions. 

Use the following command to test:
```
cfg=examples/SoftHier/assembled/Spatz_GEMM/config/arch_spatz.py app=examples/SoftHier/software/spatz_check make hs; make run
```

## Note
- The current test vectors are generated manually, and constrained with `_AVL=16`. --> [TODO] Add the compact test vector generation into `fp16_util`.