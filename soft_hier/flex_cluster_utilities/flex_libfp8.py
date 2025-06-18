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
# This script includes FP8 common functions for data generation

import numpy as np

#########################################################
#                  Format transform
# float_to_fp8_e4m3():        Scalar np.float16 --> fp8 (E4M3)
# float_to_fp8_e5m2():        Scalar np.float16 --> fp8 (E5M2)
# fp8_e5m2_to_fp16():         Scalar fp8 (E5M2) --> np.float16
# float_to_fp16():            Scalar np.float16 --> fp16 (E5M10)
# matrix_float_to_fp8_e5m2(): Matrix np.float16 --> fp8 (E5M2)
# matrix_fp8_e5m2_to_fp16():  Matrix fp8 (E5M2) --> np.float16


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

def fp8_e5m2_to_fp16(fp8):
    """Decode 8-bit FP8 E5M2 (bias=15) value into a NumPy float16."""
    fp8 = int(fp8) & 0xFF  # ensure 8-bit

    sign = (fp8 >> 7) & 0x1
    exp  = (fp8 >> 2) & 0x1F  # 5-bit exponent
    frac = fp8 & 0x3          # 2-bit mantissa

    if exp == 0 and frac == 0:
        return np.float16(-0.0 if sign else 0.0)
    elif exp == 0x1F:
        # Inf or NaN
        return np.float16('-inf') if sign else np.float16('inf')

    # Reconstruct mantissa: 1 + frac / 4.0
    mant = 1.0 + (frac / 4.0)
    exp_val = exp - 15  # bias = 15

    # Scale by 2^exp_val without math.pow
    scale = 1.0
    if exp_val >= 0:
        for _ in range(exp_val):
            scale *= 2.0
    else:
        for _ in range(-exp_val):
            scale *= 0.5

    val = mant * scale
    return np.float16(-val if sign else val)

def float_to_fp16(val):
    """Encode a float32 or float64 value into simulated fp16 (IEEE 754, E5M10) format (bias=15)."""
    f16 = np.float16(val)
    b = np.frombuffer(f16.tobytes(), dtype=np.uint16)[0]

    sign = (b >> 15) & 0x1
    exp  = (b >> 10) & 0x1F  # 5-bit exponent
    frac = b & 0x3FF         # 10-bit mantissa

    # Compose back into 16-bit integer FP16 format
    result = (sign << 15) | (exp << 10) | frac
    return result

def matrix_float_to_fp8_e5m2(val):
    """Vectorized: Encode float16 NumPy array into FP8 E5M2 (as uint8 array)."""
    f16 = val.astype(np.float16)
    b = f16.view(np.uint16)

    sign = (b >> 15) & 0x1
    exp  = (b >> 10) & 0x1F
    frac = (b >> 8)  & 0x3

    # Create empty output array
    out = np.zeros_like(exp, dtype=np.uint8)

    # Zero
    zero_mask = (exp == 0) & (frac == 0)
    out[zero_mask] = 0x00

    # Inf or NaN
    inf_mask = (exp == 0x1F)
    out[inf_mask] = np.where(sign[inf_mask] == 0, 0x7F, 0xFF)

    # Normal values
    norm_mask = ~(zero_mask | inf_mask)
    out[norm_mask] = ((sign[norm_mask] << 7) |
                      (exp[norm_mask] << 2) |
                      (frac[norm_mask]))

    return out

def matrix_fp8_e5m2_to_fp16(fp8):
    """Vectorized: Decode FP8 E5M2 NumPy array (uint8) into float16."""
    fp8 = fp8.astype(np.uint8)

    sign = (fp8 >> 7) & 0x1
    exp  = (fp8 >> 2) & 0x1F
    frac = fp8 & 0x3

    result = np.zeros_like(fp8, dtype=np.float16)

    # Zero
    zero_mask = (exp == 0) & (frac == 0)
    result[zero_mask] = np.where(sign[zero_mask], -0.0, 0.0).astype(np.float16)

    # Inf
    inf_mask = (exp == 0x1F)
    result[inf_mask] = np.where(sign[inf_mask], -np.inf, np.inf).astype(np.float16)

    # Normal values
    norm_mask = ~(zero_mask | inf_mask)
    mant = 1.0 + (frac[norm_mask] / 4.0)
    exp_val = exp[norm_mask].astype(np.int32) - 15
    scale = np.power(2.0, exp_val).astype(np.float32)
    val = mant * scale
    result[norm_mask] = np.where(sign[norm_mask], -val, val).astype(np.float16)

    return result

################################################################
#                      Data Generation
# generate_fp8_matrix(): generate a fp16-represented fp8 matrix 
#                        with specified scales

def generate_fp8_matrix(rows, cols):
    """Generate a matrix with values in the range [-1, 1], stored in float16."""
    matrix_fp16_ori = np.random.uniform(-1.0, 1.0, size=(rows, cols)).astype(np.float16)
    fp8_encoded = matrix_float_to_fp8_e5m2(matrix_fp16_ori)
    matrix_fp16_cast = matrix_fp8_e5m2_to_fp16(fp8_encoded)
    return matrix_fp16_cast.astype(np.float16)

################################################################
#                    Data Dump to Header
# write_matrix_to_header(): wrapper for data dump
#
# example usage: write_matrix_to_header(f, 'matrix_c_name', C, fmt='fp16', dtype='uint16_t')
#                write_matrix_to_header(f, 'matrix_c_name', C, fmt='e5m2', dtype='uint8_t')

def write_matrix_to_header(f, name, mat, fmt='e4m3', dtype='uint8_t'):
    """Write a flattened matrix to C header as uint8_t fp8-encoded values."""
    flat = mat.flatten()
    f.write(f'__attribute__((section(".hbm"))) static const {dtype} {name}[{len(flat)}] = {{\n')
    for i, val in enumerate(flat):
        if fmt == 'e4m3':
            int_val = float_to_fp8_e4m3(val)
            f.write(f'  0x{int_val:02X},')
        elif fmt == 'e5m2':
            int_val = float_to_fp8_e5m2(val)
            f.write(f'  0x{int_val:02X},')
        elif fmt == 'fp16':
            int_val = float_to_fp16(val)
            f.write(f'  0x{int_val:04X},')
        else:
            raise ValueError(f"Unsupported format: {fmt}")
        if (i + 1) % 8 == 0:
            f.write('\n')
    f.write('};\n\n')