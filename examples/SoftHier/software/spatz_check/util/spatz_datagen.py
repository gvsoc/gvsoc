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

def float_to_fp8_e4m3(val):
    """Encode float16 value into simulated fp8 (E4M3) format (bias=7, 4 exponent bits, 3 mantissa bits)."""
    f16 = np.float16(val)
    b = np.frombuffer(f16.tobytes(), dtype=np.uint16)[0]

    sign = (b >> 15) & 0x1
    exp  = (b >> 10) & 0x1F  # 5-bit exponent
    frac = (b >> 7)  & 0x7   # top 3 mantissa bits

    if exp == 0 and frac == 0:
        return 0x00
    elif exp == 0x1F:
        return 0x7F if sign == 0 else 0xFF  # Inf or NaN

    fp8_exp = exp - 15 + 7  # Bias shift: float16 bias=15, E4M3 bias=7
    if fp8_exp <= 0:
        return 0x00  # Underflow to zero
    elif fp8_exp >= 0xF:
        return 0x7F if sign == 0 else 0xFF  # Overflow to max

    result = (sign << 7) | ((fp8_exp & 0xF) << 3) | (frac & 0x7)
    return result

def float_to_fp8_e5m2(val):
    """Encode float16 value into simulated fp8 (E5M2) format (bias=15, 5 exponent bits, 2 mantissa bits)."""
    f16 = np.float16(val)
    b = np.frombuffer(f16.tobytes(), dtype=np.uint16)[0]

    sign = (b >> 15) & 0x1
    exp  = (b >> 10) & 0x1F  # 5-bit exponent
    frac = (b >> 8)  & 0x3   # top 2 mantissa bits

    if exp == 0 and frac == 0:
        return 0x00
    elif exp == 0x1F:
        return 0x7F if sign == 0 else 0xFF  # Inf or NaN

    result = (sign << 7) | ((exp & 0x1F) << 2) | (frac & 0x3)
    return result

def generate_fp8_matrix(rows, cols):
    """Generate a matrix with values in the range [-1, 1], stored in float16."""
    return np.random.uniform(-1.0, 1.0, size=(rows, cols)).astype(np.float16)

def write_matrix_to_header(f, name, mat, fmt='e4m3', dtype='uint8_t'):
    """Write a flattened matrix to C header as uint8_t fp8-encoded values."""
    flat = mat.flatten()
    f.write(f'static const {dtype} {name}[{len(flat)}] = {{\n')
    for i, val in enumerate(flat):
        if fmt == 'e4m3':
            int_val = float_to_fp8_e4m3(val)
        elif fmt == 'e5m2':
            int_val = float_to_fp8_e5m2(val)
        else:
            raise ValueError(f"Unsupported format: {fmt}")
        f.write(f'  0x{int_val:02X},')
        if (i + 1) % 8 == 0:
            f.write('\n')
    f.write('};\n\n')

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
    header_path = os.path.join(include_dir, 'spatz_fp8_check_data.h')

    with open(header_path, 'w') as f:
        f.write('#ifndef SPATZ_CHECK_DATA_FP8_H\n')
        f.write('#define SPATZ_CHECK_DATA_FP8_H\n\n')

        f.write(f'#define _AVL {M}\n')

        write_matrix_to_header(f, 'vector_a_fp8', A, fmt=args.format)
        write_matrix_to_header(f, 'vector_b_fp8', B, fmt=args.format)
        write_matrix_to_header(f, 'vector_c_fp8', C, fmt=args.format)
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

        f.write('#endif // SPATZ_CHECK_DATA_FP8_H\n')

    print(f'[✓] Header written to: {header_path}')

if __name__ == '__main__':
    main()
