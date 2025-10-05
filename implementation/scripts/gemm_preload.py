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
import torch
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
    # ---- params you can change ----
    seed = 0
    # --------------------------------

    torch.manual_seed(seed)

    # pick device; fp8 compute is limited, so we do compute in f16/f32 and store in fp8
    device = torch.device("cpu")

    # make sure FP8 (e5m2) is available in this PyTorch
    if not hasattr(torch, "float8_e5m2"):
        raise RuntimeError("This PyTorch build does not expose torch.float8_e5m2.")

    fptype = torch.float8_e5m2 if gemm.dtype == "fp8" else torch.float16

    # helper: create random fptype tensors by sampling in f16 then casting to fptype for storage
    def rand_fptype(shape, device):
        # sample in a safe range to reduce saturation when casting to fptype
        # normal(0, 0.5) tends to survive e5m2 rounding reasonably well
        x = torch.randn(shape, device=device, dtype=torch.float32) * 0.1 + 0.0625
        return x.to(fptype)

    compute_dtype = torch.float16

    # per-head Q,K,V in fptype
    X_fptype = rand_fptype((gemm.m_size,  gemm.k_size), device)
    W_fptype = rand_fptype((gemm.k_size,  gemm.n_size), device)
    X = X_fptype.to(compute_dtype)
    W = W_fptype.to(compute_dtype)
    Z = X @ W
    Z_fptype = Z.to(fptype)

    # Duplicate for groups
    X_list = []
    W_list = []
    Z_list = []
    if gemm.summa_group_gap_x != 0:
        for g in range(gemm.summa_group_number):
            X_list.append(X_fptype.clone())
            pass
        X_fptype = torch.cat(X_list, dim=0)
        pass
    if gemm.summa_group_gap_w != 0:
        for g in range(gemm.summa_group_number):
            W_list.append(W_fptype.clone())
            pass
        W_fptype = torch.cat(W_list, dim=0)
        pass
    if gemm.summa_group_gap_z != 0:
        for g in range(gemm.summa_group_number):
            Z_list.append(Z_fptype.clone())
            pass
        Z_fptype = torch.cat(Z_list, dim=0)
        pass

    # convert to NumPy: cast to float16 (NumPy may not support PyTorch fptype dtype)
    if fptype == torch.float8_e5m2:
        X_np = X_fptype.view(torch.uint8).cpu().numpy()
        W_np = W_fptype.view(torch.uint8).cpu().numpy()
        Z_np = Z_fptype.view(torch.uint8).cpu().numpy()
    else:
        X_np = X_fptype.view(torch.uint16).cpu().numpy()
        W_np = W_fptype.view(torch.uint16).cpu().numpy()
        Z_np = Z_fptype.view(torch.uint16).cpu().numpy()

    # quick sanity prints
    print("X_fptype:", tuple(X_fptype.shape), X_fptype.dtype)
    print("W_fptype:", tuple(W_fptype.shape), W_fptype.dtype)
    print("Z_fptype:", tuple(Z_fptype.shape), Z_fptype.dtype)

    print("X_np:", X_np.shape, X_np.dtype)
    print("W_np:", W_np.shape, W_np.dtype)
    print("Z_np:", Z_np.shape, Z_np.dtype)

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

    #generate preload elf
    if gemm.summa_numer == 1:
        pld.make_preload_elf(args.elf_path, 
            [X_np,    Z_empty,  W_np,   Z_golden],
            [X_addr,  Z_eaddr,  W_addr, Z_gaddr])
    else:
        dumnp = np.array([1,1,1,1,1])
        pld.make_preload_elf(args.elf_path, 
            [dumnp,   dumnp,    dumnp,  dumnp],
            [X_addr,  Z_eaddr,  W_addr, Z_gaddr])
        pass

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
