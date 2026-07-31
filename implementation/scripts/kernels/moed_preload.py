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


def gen_moed_preload_numpy_arrays(moed):
    # ---- params you can change ----
    seed = 0
    # --------------------------------

    torch.manual_seed(seed)

    # pick device; fp8 compute is limited, so we do compute in f16/f32 and store in fp8
    device = torch.device("cpu")

    # make sure FP8 (e5m2) is available in this PyTorch
    if not hasattr(torch, "float8_e5m2"):
        raise RuntimeError("This PyTorch build does not expose torch.float8_e5m2.")

    fptype = torch.float8_e5m2 if moed.dtype == "fp8" else torch.float16

    # helper: create random fptype tensors by sampling in f16 then casting to fptype for storage
    def rand_fptype(shape, device):
        # sample in a safe range to reduce saturation when casting to fptype
        # normal(0, 0.5) tends to survive e5m2 rounding reasonably well
        x = torch.randn(shape, device=device, dtype=torch.float32) * 0.1 + 0.0625
        return x.to(fptype)

    compute_dtype = torch.float16

    # MoEDispatch
    I_fptype = rand_fptype((moed.num_tokens,  moed.num_routed_experts), device)
    I = I_fptype.to(compute_dtype)
    V, D = torch.topk(I, moed.num_active_experts, dim=1)
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
    print("D_intype:", tuple(D_intype.shape), D_intype.dtype)

    print("I_np:", I_np.shape, I_np.dtype)
    print("D_np:", D_np.shape, D_np.dtype)

    return I_np, D_np


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate C header files from a MoEDispatch preload file and Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("elf_path", type=str, help="Path of preload elf file")
    parser.add_argument("header_path", type=str, help="Path to MoEDispatch preload C header file")
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

    # Instanciate arch and moed configurations
    moed = MoEDispatch()
    arch = FlexClusterArch()

    # Generate the preload
    I_np, D_np = gen_moed_preload_numpy_arrays(moed)
    I_addr = arch.hbm_start_base
    S_addr = I_addr + I_np.nbytes
    D_addr = arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x
    P_addr = D_addr + D_np.nbytes
    print(f"I_addr = {I_addr: #x}")
    print(f"S_addr = {S_addr: #x}")
    print(f"D_addr = {D_addr: #x}")
    print(f"P_addr = {P_addr: #x}")

    #generate preload elf
    dumnp = np.array([0,0,0,0,0])
    if moed.moed_numer == 1:
        pld.make_preload_elf(args.elf_path, 
            [I_np,    D_np,     dumnp,   dumnp,],
            [I_addr,  D_addr,   S_addr,  P_addr])
    else:
        pld.make_preload_elf(args.elf_path, 
            [dumnp,   D_np,     dumnp,   dumnp,],
            [I_addr,  D_addr,   S_addr,  P_addr])
        pass

    #generate preload header file
    with open(args.header_path, 'w') as file:
        file.write('#ifndef _MOED_PRELOAD_H_\n')
        file.write('#define _MOED_PRELOAD_H_\n\n')
        file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
        file.write(f'#define {"S_addr".upper()} ((uint64_t){S_addr: #x})\n')
        file.write(f'#define {"D_addr".upper()} ((uint64_t){D_addr: #x})\n')
        file.write(f'#define {"P_addr".upper()} ((uint64_t){P_addr: #x})\n')
        file.write('\n#endif // _MOED_PRELOAD_H_\n')

    print(f'Header file "{args.header_path}" generated successfully.')
