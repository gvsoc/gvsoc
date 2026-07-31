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
# This script generates `data_spatz_matmul_fp8.h`
# 
# Input dense matrix A (fp_8), input sparse matrix B (fp_8)
# Output dense matrix C (fp_16)

import os
import numpy as np
import argparse
from flex_libfp8 import write_matrix_to_header, generate_sparse_fp8_matrix, generate_fp8_matrix, extract_nm_sparsity, write_index_to_header_multi_format

def main():
    parser = argparse.ArgumentParser(description='FP8 Matrix Generator')
    # matrix dimension
    parser.add_argument('-M', type=int, required=True, default=16, help='Number of rows in matrix A and C')
    parser.add_argument('-N', type=int, required=True, default=16, help='Number of cols in A, rows in B')
    parser.add_argument('-P', type=int, required=True, default=16, help='Number of columns in matrix B and C')
    # sparsity format: N:M
    parser.add_argument('--sparse', action='store_true', help='Enable sparsity generation')
    parser.add_argument('-spN', type=int, default=2, help='[N]:M format')
    parser.add_argument('-spM', type=int, default=4, help='N:[M] format')
    parser.add_argument('--idx_compact', action='store_true', help='Store index compactly')
    # data format
    parser.add_argument('--input_format', choices=['e4m3', 'e5m2'], default='e5m2',
                        help='FP8 format to use: e4m3 or e5m2 (default: e5m2)')
    parser.add_argument('--output_format', choices=['e4m3', 'e5m2', 'fp16'], default='fp16',
                        help='Output format to use: fp8 [e4m3 or e5m2] | fp16 (default: fp16)')
    args = parser.parse_args()

    M, N, P = args.M, args.N, args.P
    spM, spN = args.spM, args.spN
    fmt_str = f"{spN}:{spM}"
    _sparse = args.sparse
    _idx_compact = args.idx_compact

    # Generate float16 input matrices (fp16 represented fp8 precision)
    A = generate_fp8_matrix(M, N)
    B = generate_sparse_fp8_matrix(N, P, spN, spM)
    # Compact matrix and index
    B_compact, B_index = extract_nm_sparsity(B, spN, spM)

    # Compute matrix multiplication in float16
    C = np.matmul(A.astype(np.float16), B.astype(np.float16))

    # Compute output path
    script_dir = os.path.dirname(os.path.realpath(__file__))
    include_dir = os.path.abspath(os.path.join(script_dir, '..', 'include'))
    os.makedirs(include_dir, exist_ok=True)
    header_path = os.path.join(include_dir, 'data_spatz_matmul_fp8.h')

    with open(header_path, 'w') as f:
        f.write('// This file is generated with `spatz_matmul_datagen.py`\n\n')
        f.write('#ifndef SPATZ_MATMUL_FP8_H\n')
        f.write('#define SPATZ_MATMUL_FP8_H\n\n')

        # matrix dimension
        f.write(f'#define FP8_M {M}\n')
        f.write(f'#define FP8_N {N}\n')
        f.write(f'#define FP8_P {P}\n\n')
        
        # input/output data types
        f.write(f'#define _TYPE_FP8   (1)\n')
        f.write(f'#define _TYPE_FP16  (2)\n')
        f.write(f'#define _TYPE_FP32  (4)\n')
        
        f.write(f'#define _IN_TYPE  (_TYPE_FP8)\n')
        if args.output_format == 'fp16':
            f.write(f'#define _OUT_TYPE (_TYPE_FP16)\n\n')
        else:
            f.write(f'#define _OUT_TYPE (_TYPE_FP8)\n\n')
        
        # sparsity format
        if _sparse:
            f.write(f'#define __SPARSE__\n')
            f.write(f'// structured sparsity format: spN:spM\n')
            f.write(f'#define spN {spN}\n')
            f.write(f'#define spM {spM}\n\n')
            f.write(f'#define _IDX_COMPACT (1)\n')
            f.write(f'#define _IDX_RVV     (0)\n')
            if _idx_compact:
                f.write(f'#define _IDX_TYPE (_IDX_COMPACT)\n\n')
            else:
                f.write(f'#define _IDX_TYPE (_IDX_RVV)\n\n')

        write_matrix_to_header(f, 'matrix_a_fp8', A, fmt=args.input_format)
        if not _sparse:
            write_matrix_to_header(f, 'matrix_b_fp8', B, fmt=args.input_format)
        if _sparse:
            write_matrix_to_header(f, 'matrix_b_compact_fp8', B_compact, fmt=args.input_format)
            if not _idx_compact:
                write_matrix_to_header(f, 'matrix_b_index_uint8', B_index, fmt='uint8', dtype='uint8_t')
            else:
                write_index_to_header_multi_format(f, 'matrix_b_index_compact_uint8', B_index, fmt=fmt_str)
            
        if args.output_format == 'fp16':
            write_matrix_to_header(f, 'matrix_c_fp16', C, fmt='fp16', dtype='uint16_t')
        else:
            write_matrix_to_header(f, 'matrix_c_fp8', C, fmt=args.output_format, dtype='uint8_t')

        f.write('#endif // SPATZ_MATMUL_FP8_H\n')

    print(f'[✓] Header written to: {header_path}')

if __name__ == '__main__':
    main()
