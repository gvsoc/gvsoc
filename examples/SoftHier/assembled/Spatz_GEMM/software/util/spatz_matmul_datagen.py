#!/usr/bin/env python3
# Copyright 2025 ETH Zurich and University of Bologna.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Author: Bowen Wang, ETH Zurich
# This script generates fp8_matmul input/output header file

import os
import numpy as np
import argparse
from flex_libfp8 import generate_fp8_matrix, write_matrix_to_header

def main():
    parser = argparse.ArgumentParser(description='FP8 Matrix Generator')
    parser.add_argument('-M', type=int, required=True, help='Number of rows in matrix A and C')
    parser.add_argument('-N', type=int, required=True, help='Number of cols in A, rows in B')
    parser.add_argument('-P', type=int, required=True, help='Number of columns in matrix B and C')
    parser.add_argument('--input_format', choices=['e4m3', 'e5m2'], default='e5m2',
                        help='FP8 format to use: e4m3 or e5m2 (default: e5m2)')
    parser.add_argument('--output_format', choices=['e4m3', 'e5m2', 'fp16'], default='fp16',
                        help='Output format to use: fp8 [e4m3 or e5m2] | fp16 (default: fp16)')
    args = parser.parse_args()

    M, N, P = args.M, args.N, args.P

    # Generate float16 input matrices
    A = generate_fp8_matrix(M, N)
    B = generate_fp8_matrix(N, P)

    # Compute matrix multiplication in float16
    C = np.matmul(A.astype(np.float16), B.astype(np.float16))

    # Compute output path
    script_dir = os.path.dirname(os.path.realpath(__file__))
    include_dir = os.path.abspath(os.path.join(script_dir, '..', 'include'))
    os.makedirs(include_dir, exist_ok=True)
    header_path = os.path.join(include_dir, 'data_spatz_matmul_fp8.h')

    with open(header_path, 'w') as f:
        f.write('#ifndef SPATZ_MATMUL_FP8_H\n')
        f.write('#define SPATZ_MATMUL_FP8_H\n\n')

        f.write(f'#define FP8_M {M}\n')
        f.write(f'#define FP8_N {N}\n')
        f.write(f'#define FP8_P {P}\n\n')

        write_matrix_to_header(f, 'matrix_a_fp8', A, fmt=args.input_format)
        write_matrix_to_header(f, 'matrix_b_fp8', B, fmt=args.input_format)
        if args.output_format == 'fp16':
            write_matrix_to_header(f, 'matrix_c_fp16', C, fmt='fp16', dtype='uint16_t')
        else:
            write_matrix_to_header(f, 'matrix_c_fp8', C, fmt=args.output_format, dtype='uint8_t')

        f.write('#endif // SPATZ_MATMUL_FP8_H\n')

    print(f'[✓] Header written to: {header_path}')

if __name__ == '__main__':
    main()
