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
# This script generates fp8 input/output header file

import os
import numpy as np
import argparse
from flex_libfp8 import generate_fp8_matrix, write_matrix_to_header

def main():
    parser = argparse.ArgumentParser(description='FP8 Tensor Generator')
    parser.add_argument('-M', type=int, required=True, help='Number of elements (_AVL)')
    parser.add_argument('--format', choices=['e4m3', 'e5m2'], default='e5m2',
                        help='FP8 format to use: e4m3 or e5m2 (default: e5m2)')
    args = parser.parse_args()

    M = args.M

    # Generate float16 input vectors
    A = generate_fp8_matrix(M, 1)
    B = generate_fp8_matrix(M, 1)
    C = generate_fp8_matrix(M, 1)

    # Compute in float16
    C_add_vv = np.add(A.astype(np.float32), B.astype(np.float32))      # C = A + B
    C_sub_vv = np.subtract(A.astype(np.float16), B.astype(np.float16)) # C = A - B
    C_min_vv = np.minimum(A.astype(np.float16), B.astype(np.float16))  # elementwise min
    C_max_vv = np.maximum(A.astype(np.float16), B.astype(np.float16))  # elementwise max
    C_mul_vv = np.multiply(A.astype(np.float16), B.astype(np.float16)) # C = A x B
    C_madd_vv = np.add(np.multiply(A.astype(np.float16), C.astype(np.float16)), B.astype(np.float16)) # A * B + C
    C_macc_vv = np.add(np.multiply(A.astype(np.float16), B.astype(np.float16)), C.astype(np.float16)) # A * B + C_madd
    
    # vector scalar
    S = generate_fp8_matrix(1, 1)
    C_add_vf = np.add(A.astype(np.float16), S.astype(np.float16))      # C = A + S
    C_sub_vf = np.subtract(A.astype(np.float16), S.astype(np.float16)) # C = A - S
    C_mul_vf = np.multiply(A.astype(np.float16), S.astype(np.float16)) # C = A x S
    C_madd_vf = np.add(np.multiply(C.astype(np.float16), S.astype(np.float16)), A.astype(np.float16)) # A * S + C
    C_macc_vf = np.add(np.multiply(A.astype(np.float16), S.astype(np.float16)), C.astype(np.float16)) # A * S + C_madd

    # Compute output path
    script_dir = os.path.dirname(os.path.realpath(__file__))
    include_dir = os.path.abspath(os.path.join(script_dir, '..', 'include'))
    os.makedirs(include_dir, exist_ok=True)
    header_path = os.path.join(include_dir, 'data_spatz_fp8_check.h')

    with open(header_path, 'w') as f:
        f.write('#ifndef SPATZ_CHECK_DATA_FP8_H\n')
        f.write('#define SPATZ_CHECK_DATA_FP8_H\n\n')

        f.write(f'#define _AVL {M}\n')

        write_matrix_to_header(f, 'vector_a_fp8', A, fmt=args.format)
        write_matrix_to_header(f, 'vector_b_fp8', B, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_fp8', C, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_fp16', C, fmt='fp16', dtype='uint16_t') # for widening instructions
        write_matrix_to_header(f, 's_fp8', S, fmt=args.format)
        
        write_matrix_to_header(f, 'vector_c_add_vv_fp8', C_add_vv, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_sub_vv_fp8', C_sub_vv, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_min_vv_fp8', C_min_vv, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_max_vv_fp8', C_max_vv, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_mul_vv_fp8', C_mul_vv, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_madd_vv_fp8', C_madd_vv, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_macc_vv_fp8', C_macc_vv, fmt=args.format)
        
        write_matrix_to_header(f, 'vector_c_add_vf_fp8', C_add_vf, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_sub_vf_fp8', C_sub_vf, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_mul_vf_fp8', C_mul_vf, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_madd_vf_fp8', C_madd_vf, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_macc_vf_fp8', C_macc_vf, fmt=args.format)
        
        # widening instructions
        write_matrix_to_header(f, 'vector_c_wadd_vf_fp8', C_add_vf, fmt='fp16', dtype='uint16_t')
        write_matrix_to_header(f, 'vector_c_wsub_vf_fp8', C_sub_vf, fmt='fp16', dtype='uint16_t')
        write_matrix_to_header(f, 'vector_c_wmul_vf_fp8', C_mul_vf, fmt='fp16', dtype='uint16_t')
        write_matrix_to_header(f, 'vector_c_wmacc_vf_fp8', C_macc_vf, fmt='fp16', dtype='uint16_t')

        f.write('#endif // SPATZ_CHECK_DATA_FP8_H\n')

    print(f'[✓] Header written to: {header_path}')

if __name__ == '__main__':
    main()
