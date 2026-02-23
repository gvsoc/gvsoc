#
# Copyright (C) 2025 ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Author: Chi Zhang <chizhang@ethz.ch>

import re
import os
import io
import sys
import math
import shutil
import argparse
import numpy as np
import importlib.util
from tqdm import tqdm
import preload as pld

def import_module_from_path(module_path):
    """
    Dynamically import a module from an absolute path and mimic `from module import *`.
    """
    module_name = os.path.splitext(os.path.basename(module_path))[0]  # Extract the file name without extension
    spec = importlib.util.spec_from_file_location(module_name, module_path)
    if spec is None:
        raise ImportError(f"Cannot find a module at path: {module_path}")
    
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    
    # Mimic `from module import *`
    globals().update(vars(module))
    return module

def gen_gemm_preload_numpy_arrays(gemm):
    dtype = np.uint8 if gemm.dtype == "fp8" else np.uint16

    # Activation
    X_np = np.ones((gemm.m_size,  gemm.k_size), dtype=dtype)
    W_np = np.ones((gemm.k_size,  gemm.n_size), dtype=dtype)
    Z_np = np.ones((gemm.m_size,  gemm.n_size), dtype=dtype)

    Z_empty = np.zeros(Z_np.shape, dtype=Z_np.dtype)
    Z_golden = Z_np

    return X_np, W_np, Z_empty, Z_golden


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate C header files from a GEMM preload file and Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("elf_path", type=str, help="Path of preload elf file")
    parser.add_argument("header_path", type=str, help="Path to GEMM preload C header file")
    parser.add_argument(
        "module_paths",
        metavar="module_path",
        type=str,
        nargs="+",
        help="Paths to Python modules to import (absolute or relative)."
    )

    args = parser.parse_args()

    # Process each provided module path
    for module_path in args.module_paths:
        # Convert to absolute path
        absolute_path = os.path.abspath(module_path)

        if not os.path.isfile(absolute_path):
            print(f"Error: {absolute_path} is not a valid file.")
            continue

        try:
            # Import the module dynamically
            module = import_module_from_path(absolute_path)
            print(f"Successfully imported: {module.__name__} from {absolute_path}")
        except Exception as e:
            print(f"Failed to import {absolute_path}: {e}")

    # Instanciate arch and gemm configurations
    gemm = SummaGEMM()
    arch = FlexClusterArch()

    # Generate the preload
    X_np, W_np, Z_empty, Z_golden = gen_gemm_preload_numpy_arrays(gemm)
    X_addr = arch.hbm_start_base
    W_addr = arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x
    Z_eaddr = X_addr + X_np.nbytes
    Z_gaddr = W_addr + W_np.nbytes
    print(f"X_addr = {X_addr: #x}")
    print(f"Z_eaddr = {Z_eaddr: #x}")
    print(f"W_addr = {W_addr: #x}")
    print(f"Z_gaddr = {Z_gaddr: #x}")

    dumnp = np.array([1,1,1,1,1])
    pld.make_preload_elf(args.elf_path, 
        [dumnp,   dumnp,    dumnp,  dumnp],
        [X_addr,  Z_eaddr,  W_addr, Z_gaddr])

    #generate preload header file
    with open(args.header_path, 'w') as file:
        file.write('#ifndef _GEMM_PRELOAD_H_\n')
        file.write('#define _GEMM_PRELOAD_H_\n\n')
        file.write(f'#define {"X_addr".upper()} ((uint64_t){X_addr: #x})\n')
        file.write(f'#define {"Z_eaddr".upper()} ((uint64_t){Z_eaddr: #x})\n')
        file.write(f'#define {"W_addr".upper()} ((uint64_t){W_addr: #x})\n')
        file.write(f'#define {"Z_gaddr".upper()} ((uint64_t){Z_gaddr: #x})\n')
        file.write('\n#endif // _GEMM_PRELOAD_H_\n')

    print(f'Header file "{args.header_path}" generated successfully.')
