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


def gen_moec_preload_numpy_arrays(moec):
    # ---- params you can change ----
    seed = 0
    # --------------------------------

    torch.manual_seed(seed)

    # pick device; fp8 compute is limited, so we do compute in f16/f32 and store in fp8
    device = torch.device("cpu")

    # make sure FP8 (e5m2) is available in this PyTorch
    if not hasattr(torch, "float8_e5m2"):
        raise RuntimeError("This PyTorch build does not expose torch.float8_e5m2.")

    fptype = torch.float8_e5m2 if moec.dtype == "fp8" else torch.float16

    # helper: create random fptype tensors by sampling in f16 then casting to fptype for storage
    def rand_fptype(shape, device):
        # sample in a safe range to reduce saturation when casting to fptype
        # normal(0, 0.5) tends to survive e5m2 rounding reasonably well
        x = torch.randn(shape, device=device, dtype=torch.float32) * 0.1 + 0.0625
        return x.to(fptype)

    compute_dtype = torch.float16

    # Generate MoE Distribution Example
    T_fptype = rand_fptype((moec.num_tokens,  moec.num_routed_experts), device)
    T = T_fptype.to(compute_dtype)
    V, D = torch.topk(T, moec.num_active_experts, dim=1)
    P = torch.zeros(D.shape, dtype=D.dtype)
    SCORE = torch.zeros(moec.num_routed_experts, dtype=torch.int32)
    Expert_token_list = []
    for i in tqdm(range(D.shape[0]), desc="[Expert Positioning]"):
        for j in range(D.shape[1]):
            #1. get expert idx
            expert_idx = D[i, j]
            #2. get position
            pos = SCORE[expert_idx].clone()
            SCORE[expert_idx] += 1
            #3. store position
            P[i, j] = pos
            pass
        pass
    for i in SCORE:
        Expert_token_list.append(rand_fptype((i,  moec.embedded_length), device))
        pass
    O = torch.zeros((moec.num_tokens,  moec.embedded_length), dtype=fptype)
    for i in tqdm(range(D.shape[0]), desc="[MoE Combine]"):
        for j in range(D.shape[1]):
            #1. get factor
            factor = V[i, j]
            #2. get expert index
            expert_idx = D[i, j]
            #3. get position index
            pos_idx = P[i, j]
            #4. take the corresponding token
            token = Expert_token_list[expert_idx][pos_idx]
            #5. update the Output
            O[i] += factor * token
            pass
        pass
    V_fptype = V.to(fptype)
    O_fptype = O.to(fptype)
    D_intype = D.to(torch.int32)
    P_intype = P.to(torch.int32)

    # convert to NumPy: cast to float16 (NumPy may not support PyTorch fptype dtype)
    if fptype == torch.float8_e5m2:
        Expert_token_list = [e.view(torch.uint8).cpu().numpy() for e in Expert_token_list]
        V_np = V_fptype.view(torch.uint8).cpu().numpy()
        O_np = O_fptype.view(torch.uint8).cpu().numpy()
    else:
        Expert_token_list = [e.view(torch.uint16).cpu().numpy() for e in Expert_token_list]
        V_np = V_fptype.view(torch.uint16).cpu().numpy()
        O_np = O_fptype.view(torch.uint16).cpu().numpy()
    D_np = D_intype.cpu().numpy()
    P_np = P_intype.cpu().numpy()

    print("Expert_token_list:", [e.shape for e in Expert_token_list])
    print("V_np:", V_np.shape, V_np.dtype)
    print("D_np:", D_np.shape, D_np.dtype)
    print("P_np:", P_np.shape, P_np.dtype)
    print("O_np:", O_np.shape, O_np.dtype)

    O_empty = np.zeros(O_np.shape, dtype=O_np.dtype)
    O_golden = O_np

    return Expert_token_list, V_np, D_np, P_np, O_empty, O_golden


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate C header files from a MoECombine preload file and Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("elf_path", type=str, help="Path of preload elf file")
    parser.add_argument("header_path", type=str, help="Path to MoECombine preload C header file")
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

    # Instanciate arch and moec configurations
    moec = MoECombine()
    arch = FlexClusterArch()

    # Generate the preload
    Expert_token_list, V_np, D_np, P_np, O_empty, O_golden = gen_moec_preload_numpy_arrays(moec)
    I_addr  = arch.hbm_start_base
    V_addr  = arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x
    D_addr  = V_addr  + V_np.nbytes
    P_addr  = D_addr  + D_np.nbytes
    O_eaddr = P_addr  + P_np.nbytes
    O_gaddr = O_eaddr + O_empty.nbytes
    print(f"I_addr  = {I_addr: #x}")
    print(f"V_addr  = {V_addr: #x}")
    print(f"D_addr  = {D_addr: #x}")
    print(f"P_addr  = {P_addr: #x}")
    print(f"O_eaddr = {O_eaddr: #x}")
    print(f"O_gaddr = {O_gaddr: #x}")

    #generate preload elf
    dumnp = np.array([0,0,0,0,0])
    elem_size = 1 if moec.dtype == 'fp8' else 2
    Expert_buffer_addr_list = [I_addr + i * moec.num_tokens * moec.embedded_length * elem_size for i in range(moec.num_routed_experts)]
    if moec.moec_numer == 1:
        pld.make_preload_elf(args.elf_path, 
            Expert_token_list       + [V_np,    D_np,     P_np,    O_empty,  O_golden],
            Expert_buffer_addr_list + [V_addr,  D_addr,   P_addr,  O_eaddr,  O_gaddr])
    else:
        pld.make_preload_elf(args.elf_path, 
            [V_np,    D_np,     P_np],
            [V_addr,  D_addr,   P_addr])
        pass

    #generate preload header file
    with open(args.header_path, 'w') as file:
        file.write('#ifndef _MOEC_PRELOAD_H_\n')
        file.write('#define _MOEC_PRELOAD_H_\n\n')
        file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
        file.write(f'#define {"V_addr".upper()} ((uint64_t){V_addr: #x})\n')
        file.write(f'#define {"D_addr".upper()} ((uint64_t){D_addr: #x})\n')
        file.write(f'#define {"P_addr".upper()} ((uint64_t){P_addr: #x})\n')
        file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
        file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
        file.write('\n#endif // _MOEC_PRELOAD_H_\n')

    print(f'Header file "{args.header_path}" generated successfully.')
