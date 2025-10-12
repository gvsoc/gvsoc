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


def gen_moeg_preload_numpy_arrays(moeg):
    # ---- params you can change ----
    seed = 0
    # --------------------------------

    torch.manual_seed(seed)

    # pick device; fp8 compute is limited, so we do compute in f16/f32 and store in fp8
    device = torch.device("cpu")

    # make sure FP8 (e5m2) is available in this PyTorch
    if not hasattr(torch, "float8_e5m2"):
        raise RuntimeError("This PyTorch build does not expose torch.float8_e5m2.")

    fptype = torch.float8_e5m2 if moeg.dtype == "fp8" else torch.float16

    # helper: create random fptype tensors by sampling in f16 then casting to fptype for storage
    def rand_fptype(shape, device):
        # sample in a safe range to reduce saturation when casting to fptype
        # normal(0, 0.5) tends to survive e5m2 rounding reasonably well
        x = torch.randn(shape, device=device, dtype=torch.float32) * 0.05 + 0.0625
        return x.to(fptype)

    compute_dtype = torch.float16

    # MoEGate
    I_fptype = rand_fptype((moeg.num_tokens,  moeg.num_routed_experts), device)
    I = I_fptype.to(compute_dtype)
    V, D = torch.topk(I, moeg.num_active_experts, dim=1)
    V_fptype = V.to(fptype)
    D_intype = D.to(torch.int32)

    # convert to NumPy: cast to float16 (NumPy may not support PyTorch fptype dtype)
    if fptype == torch.float8_e5m2:
        I_np = I_fptype.view(torch.uint8).cpu().numpy()
        V_np = V_fptype.view(torch.uint8).cpu().numpy()
    else:
        I_np = I_fptype.view(torch.uint16).cpu().numpy()
        V_np = V_fptype.view(torch.uint16).cpu().numpy()
    D_np = D_intype.cpu().numpy()

    # quick sanity prints
    print("I_fptype:", tuple(I_fptype.shape), I_fptype.dtype)
    print("V_fptype:", tuple(V_fptype.shape), V_fptype.dtype)
    print("D_intype:", tuple(D_intype.shape), D_intype.dtype)

    print("I_np:", I_np.shape, I_np.dtype)
    print("V_np:", V_np.shape, V_np.dtype)
    print("D_np:", D_np.shape, D_np.dtype)

    V_empty = np.zeros(V_np.shape, dtype=V_np.dtype)
    V_golden = V_np

    D_empty = np.zeros(D_np.shape, dtype=D_np.dtype)
    D_golden = D_np

    return I_np, V_empty, V_golden, D_empty, D_golden


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate C header files from a MoEGate preload file and Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("elf_path", type=str, help="Path of preload elf file")
    parser.add_argument("header_path", type=str, help="Path to MoEGate preload C header file")
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

    # Instanciate arch and moeg configurations
    moeg = MoEGate()
    arch = FlexClusterArch()

    # Generate the preload
    assert moeg.token_per_cluster < arch.num_core_per_cluster
    I_np, V_empty, V_golden, D_empty, D_golden = gen_moeg_preload_numpy_arrays(moeg)
    I_addr = arch.hbm_start_base
    V_eaddr = I_addr + I_np.nbytes
    V_gaddr = V_eaddr + V_empty.nbytes
    D_eaddr = V_gaddr + V_golden.nbytes
    D_gaddr = D_eaddr + D_empty.nbytes
    print(f"I_addr = {I_addr: #x}")
    print(f"V_eaddr = {V_eaddr: #x}")
    print(f"V_gaddr = {V_gaddr: #x}")
    print(f"D_eaddr = {D_eaddr: #x}")
    print(f"D_gaddr = {D_gaddr: #x}")

    #generate preload elf
    if moeg.moeg_numer == 1:
        pld.make_preload_elf(args.elf_path, 
            [I_np,    V_empty,  V_golden,  D_empty,  D_golden],
            [I_addr,  V_eaddr,  V_gaddr,   D_eaddr,  D_gaddr])
    else:
        dumnp = np.array([1,1,1,1,1])
        pld.make_preload_elf(args.elf_path, 
            [dumnp,   dumnp,    dumnp,     dumnp,    dumnp],
            [I_addr,  V_eaddr,  V_gaddr,   D_eaddr,  D_gaddr])
        pass

    #generate preload header file
    with open(args.header_path, 'w') as file:
        file.write('#ifndef _MOEG_PRELOAD_H_\n')
        file.write('#define _MOEG_PRELOAD_H_\n\n')
        file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
        file.write(f'#define {"V_eaddr".upper()} ((uint64_t){V_eaddr: #x})\n')
        file.write(f'#define {"V_gaddr".upper()} ((uint64_t){V_gaddr: #x})\n')
        file.write(f'#define {"D_eaddr".upper()} ((uint64_t){D_eaddr: #x})\n')
        file.write(f'#define {"D_gaddr".upper()} ((uint64_t){D_gaddr: #x})\n')
        file.write('\n#endif // _MOEG_PRELOAD_H_\n')

    print(f'Header file "{args.header_path}" generated successfully.')
