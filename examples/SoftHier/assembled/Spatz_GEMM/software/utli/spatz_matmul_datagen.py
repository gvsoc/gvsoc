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
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Author: Bowen Wang, ETH Zurich
# This script generates fp8_matmul input/output header file

import os
import numpy as np
import argparse

def float_to_fp8_e4m3(val):
    """
    Encode float16 value into simulated fp8 (E4M3) format.
    Returns: uint8
    """
    f16 = np.float16(val)
    b = np.frombuffer(f16.tobytes(), dtype=np.uint16)[0]

    sign = (b >> 15) & 0x1
    exp  = (b >> 10) & 0x1F  # 5 bits
    frac = (b >> 7) & 0x7    # top 3 mantissa bits

    if exp == 0 and frac == 0:
        return 0x00  # zero
    elif exp == 0x1F:
        return 0x7F if sign == 0 else 0xFF  # Inf or NaN

    # Bias difference: fp16 bias = 15, fp8 bias = 7
    fp8_exp = exp - 15 + 7
    if fp8_exp <= 0:
        return 0x00  # underflow to zero
    elif fp8_exp >= 0xF:
        return 0x7F if sign == 0 else 0xFF  # overflow to max

    result = (sign << 7) | ((fp8_exp & 0xF) << 3) | (frac & 0x7)
    return result

def generate_fp8_matrix(rows, cols):
    return np.random.uniform(-1.0, 1.0, size=(rows, cols)).astype(np.float16)

def write_matrix_to_header(f, name, mat, dtype='uint8_t'):
    flat = mat.flatten()
    f.write(f'static const {dtype} {name}[{len(flat)}] = {{\n')
    for i, val in enumerate(flat):
        int_val = float_to_fp8_e4m3(val)
        f.write(f'  0x{int_val:02X},')
        if (i + 1) % 8 == 0:
            f.write('\n')
    f.write('};\n\n')

def main():
    parser = argparse.ArgumentParser(description='FP8 Matrix Generator')
    parser.add_argument('-M', type=int, required=True)
    parser.add_argument('-N', type=int, required=True)
    parser.add_argument('-P', type=int, required=True)
    args = parser.parse_args()

    M, N, P = args.M, args.N, args.P

    A = generate_fp8_matrix(M, N)
    B = generate_fp8_matrix(N, P)
    C = np.zeros((M, P), dtype=np.float16)

    # Compute absolute include path
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

        write_matrix_to_header(f, 'matrix_a_fp8', A)
        write_matrix_to_header(f, 'matrix_b_fp8', B)
        write_matrix_to_header(f, 'matrix_c_fp8', C)

        f.write('#endif // SPATZ_MATMUL_FP8_H\n')

if __name__ == '__main__':
    main()