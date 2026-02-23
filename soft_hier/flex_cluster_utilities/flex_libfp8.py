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
import math

#################################################################
#                     Format transform
# float_to_fp8_e4m3():        Scalar np.float16 --> fp8 (E4M3)
# float_to_fp8_e5m2():        Scalar np.float16 --> fp8 (E5M2)
# fp8_e5m2_to_fp16():         Scalar fp8 (E5M2) --> np.float16
# float_to_fp16():            Scalar np.float16 --> fp16 (E5M10)
# matrix_float_to_fp8_e5m2(): Matrix np.float16 --> fp8 (E5M2)
# matrix_fp8_e5m2_to_fp16():  Matrix fp8 (E5M2) --> np.float16
#################################################################


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

#############################################################################
#                           Data Generation
# generate_fp8_matrix():        generate a fp16-represented fp8 matrix 
#                               with specified scales
#
# generate_nm_sparse_matrix():  generate a fp16 sparse matrix
#
# generate_sparse_fp8_matrix(): wrapper function to cast sparse fp16 matrix 
#                               to fp8 precision
#
# extract_nm_sparsity():        take a sparse matrix as input and generate 
#                               the compact layout and indices
#############################################################################


def generate_fp8_matrix(rows, cols):
    """Generate a matrix with values in the range [-1, 1], stored in float16."""
    matrix_fp16_ori = np.random.uniform(-1.0, 1.0, size=(rows, cols)).astype(np.float16)
    fp8_encoded = matrix_float_to_fp8_e5m2(matrix_fp16_ori)
    matrix_fp16_cast = matrix_fp8_e5m2_to_fp16(fp8_encoded)
    return matrix_fp16_cast.astype(np.float16)

def generate_nm_sparse_matrix(rows, cols, N, M, seed=None):
    assert cols % M == 0, "Each row must be divisible by M"
    if seed is not None:
        np.random.seed(seed)
    
    matrix = np.zeros((rows, cols), dtype=np.float16)
    
    for i in range(rows):
        for j in range(0, cols, M):
            # Choose N random positions out of M
            nz_indices = np.random.choice(M, N, replace=False)
            # Assign random values to these N positions
            block_values = np.zeros(M, dtype=np.float16)
            block_values[nz_indices] = np.random.randn(N)
            # Fill the block into the matrix
            matrix[i, j:j+M] = block_values
            
    return matrix

def generate_sparse_fp8_matrix(rows, cols, N, M):
    """Cast the fp16 sparse matrix to fp8 precision."""
    matrix_fp16_ori = generate_nm_sparse_matrix(rows, cols, N, M).astype(np.float16)
    fp8_encoded = matrix_float_to_fp8_e5m2(matrix_fp16_ori)
    matrix_fp16_cast = matrix_fp8_e5m2_to_fp16(fp8_encoded)
    return matrix_fp16_cast.astype(np.float16)

def extract_nm_sparsity(matrix, N, M):
    rows, cols = matrix.shape
    assert cols % M == 0, "Each row must be divisible by M"
    
    num_blocks_per_row = cols // M
    compact_vals = []
    index_matrix = np.zeros((rows, num_blocks_per_row * N), dtype=np.uint8)
    
    for i in range(rows):
        idx_offset = 0
        for j in range(0, cols, M):
            block = matrix[i, j:j+M]
            nz_indices = np.nonzero(block)[0]
            nz_values = block[nz_indices]

            assert len(nz_indices) == N, f"Block at row {i}, col {j} does not have exactly {N} non-zeros"
            
            compact_vals.extend(nz_values)
            index_matrix[i, idx_offset:idx_offset+N] = nz_indices.astype(np.uint8)
            idx_offset += N
    
    compact_vals = np.array(compact_vals, dtype=np.float16)
    return compact_vals, index_matrix

###############################################################################################
#                                      Data Dump to Header
# write_matrix_to_header(): wrapper for data dump
#
# example usage: write_matrix_to_header(f, 'matrix_c_name', C, fmt='fp16', dtype='uint16_t')
#                write_matrix_to_header(f, 'matrix_c_name', C, fmt='e5m2', dtype='uint8_t')
###############################################################################################

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
        elif fmt == 'uint8':
            int_val = val
            f.write(f'  0x{int_val:02X},')
        else:
            raise ValueError(f"Unsupported format: {fmt}")
        if (i + 1) % 8 == 0:
            f.write('\n')
    f.write('};\n\n')
    
    
def write_index_to_header(f, name, mat, fmt='nm2bit', dtype='uint8_t'):
    """Write a flattened matrix to C header with optional compact formats."""
    flat = mat.flatten()
    
    if fmt == 'nm2bit':
        # Check that number of elements is divisible by 4
        assert len(flat) % 4 == 0, "Length of index matrix must be divisible by 4 for 2-bit packing"
        f.write(f'#define _IDX_PER_BYTE (4)\n\n')

        packed = []
        for i in range(0, len(flat), 4):
            b0 = flat[i]   & 0x03
            b1 = flat[i+1] & 0x03
            b2 = flat[i+2] & 0x03
            b3 = flat[i+3] & 0x03
            byte = (b3 << 6) | (b2 << 4) | (b1 << 2) | b0
            packed.append(byte)

        f.write(f'__attribute__((section(".hbm"))) static const uint8_t {name}[{len(packed)}] = {{\n')
        for i, val in enumerate(packed):
            f.write(f'  0x{val:02X},')
            if (i + 1) % 8 == 0:
                f.write('\n')
        f.write('};\n\n')

    else:
        raise ValueError(f"Unsupported format: {fmt}")

def _pack_bits(values, bits):
    """Pack a list/array of non-negative integers into bytes using `bits` bits per value (LSB-first)."""
    if bits < 1 or bits > 8:
        raise ValueError("bits must be between 1 and 8")
    out = []
    acc = 0
    acc_bits = 0
    mask = (1 << bits) - 1

    for v in values:
        if v < 0 or v > mask:
            raise ValueError(f"value {v} does not fit in {bits} bits")
        acc |= (v & mask) << acc_bits
        acc_bits += bits
        while acc_bits >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            acc_bits -= 8

    if acc_bits > 0:
        out.append(acc & 0xFF)
    return out

def _bits_for_fmt(fmt, flat):
    """Resolve bits-per-index from fmt string, with X:8 forced to 4 bits."""
    fmt = str(fmt).lower().strip()

    # Direct N:M formats
    nm_map = {
        "1:2": 1,
        "1:4": 2, "2:4": 2,
        "1:8": 4, "2:8": 4, "4:8": 4,   # forced 4-bit for X:8
        "1:16": 4, "2:16": 4, "4:16": 4, "8:16": 4,
    }
    if fmt in nm_map:
        return nm_map[fmt]

    # Legacy/explicit bit formats
    if fmt in ("nm1bit", "1bit"):
        return 1
    if fmt in ("nm2bit", "2bit"):
        return 2
    if fmt in ("nm4bit", "4bit"):
        return 4

    if fmt == "auto":
        # Auto-detect, but cap to 4 bits (your supported max)
        maxv = 0 if len(flat) == 0 else max(flat)
        bits = max(1, math.ceil(math.log2(maxv + 1))) if maxv > 0 else 1
        if bits > 4:
            raise ValueError(f"auto-detected {bits} bits (>4). Expected <= 4.")
        return bits

    raise ValueError(f"Unsupported format: {fmt}")

def write_index_to_header_multi_format(f, name, mat, fmt='auto', array_c_type='uint8_t', section='.hbm'):
    """
    Write a flattened index matrix to a C header as packed bytes.

    Supported formats:
      - '1:2'  -> 1 bit/index
      - '1:4','2:4' -> 2 bits/index
      - '1:8','2:8','4:8' -> **4 bits/index (forced for hardware)**
      - '1:16' -> 4 bits/index
      - Legacy: 'nm1bit'..'nm4bit', 'auto' (detects up to 4 bits)
    """
    flat = mat.flatten().tolist()
    bits = _bits_for_fmt(fmt, flat)

    packed = _pack_bits(flat, bits)

    # Emit macros
    f.write(f'#define _IDX_BITS_PER_INDEX ({bits})\n')
    if 8 % bits == 0:
        f.write(f'#define _IDX_PER_BYTE ({8 // bits})\n')
    f.write('\n')

    # Byte array
    f.write(f'__attribute__((section("{section}"))) '
            f'static const uint8_t {name}[{len(packed)}] = {{\n')
    for i, val in enumerate(packed):
        f.write(f'  0x{val:02X},')
        if (i + 1) % 16 == 0:
            f.write('\n')
    f.write('\n};\n\n')

    # Optional alias
    f.write(f'static const {array_c_type} * const {name}_data = {name};\n\n')

