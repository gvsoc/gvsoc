#!/bin/bash

# List of P values to test
P_VALUES=(32 128 512 1024)

for P in "${P_VALUES[@]}"; do
  echo "Running with P=$P"

  # Run data generation script
  python examples/SoftHier/assembled/Spatz_GEMM/software/util/spatz_matmul_datagen.py \
    -M 32 -N 32 -P $P -spN 2 -spM 4 --sparse --idx_compact

  # Run make commands
  cfg=examples/SoftHier/assembled/Spatz_GEMM/config/arch_spatz.py \
  app=examples/SoftHier/assembled/Spatz_GEMM/software \
  make hs; make run
done