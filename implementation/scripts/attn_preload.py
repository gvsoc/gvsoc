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

def gen_attention_preload_numpy_arrays(b, h, d, sq, skv, dt):
    # ---- params you can change ----
    seed = 0
    # --------------------------------

    torch.manual_seed(seed)

    # pick device; fp8 compute is limited, so we do compute in f16/f32 and store in fp8
    device = torch.device("cpu")

    # make sure FP8 (e5m2) is available in this PyTorch
    if not hasattr(torch, "float8_e5m2"):
        raise RuntimeError("This PyTorch build does not expose torch.float8_e5m2.")

    fptype = torch.float8_e5m2 if dt == "fp8" else torch.float16

    # helper: create random fptype tensors by sampling in f16 then casting to fptype for storage
    def rand_fptype(shape, device):
        # sample in a safe range to reduce saturation when casting to fptype
        # normal(0, 0.5) tends to survive e5m2 rounding reasonably well
        x = torch.randn(shape, device=device, dtype=torch.float16) * 0.5
        return x.to(fptype)

    # per-head Q,K,V in fptype
    Qs = []
    Ks = []
    Vs = []
    Os = []

    for _ in range(b * h):
        Q_fptype = rand_fptype((sq,  d), device)
        K_fptype = rand_fptype((skv, d), device)
        V_fptype = rand_fptype((skv, d), device)

        # compute attention in higher precision, then (optionally) store O in fptype
        # cast up for matmuls/softmax stability
        # - on CUDA, float16 or bfloat16 is fine; float32 is safest everywhere.
        compute_dtype = torch.float16

        Q = Q_fptype.to(compute_dtype)
        K = K_fptype.to(compute_dtype)
        V = V_fptype.to(compute_dtype)

        attn_scores = (Q @ K.transpose(0, 1)) / math.sqrt(d)     # [sq, skv]
        attn_probs  = torch.softmax(attn_scores, dim=-1)         # [sq, skv]
        O = attn_probs @ V                                       # [sq, d]

        # store results: keep Q,K,V in fptype as requested; also provide O in fptype
        O_fptype = O.to(fptype)

        Qs.append(Q_fptype)
        Ks.append(K_fptype)
        Vs.append(V_fptype)
        Os.append(O_fptype)

    # concatenate heads:
    # Q, O -> [sq, h*d]; K, V -> [skv, h*d]
    Q_cat_fptype = torch.cat(Qs, dim=0)
    K_cat_fptype = torch.cat(Ks, dim=0)
    V_cat_fptype = torch.cat(Vs, dim=0)
    O_cat_fptype = torch.cat(Os, dim=0)

    # convert to NumPy: cast to float16 (NumPy may not support PyTorch fptype dtype)
    if fptype == torch.float8_e5m2:
        Q_np = Q_cat_fptype.view(torch.uint8).cpu().numpy()
        K_np = K_cat_fptype.view(torch.uint8).cpu().numpy()
        V_np = V_cat_fptype.view(torch.uint8).cpu().numpy()
        O_np = O_cat_fptype.view(torch.uint8).cpu().numpy()
    else:
        Q_np = Q_cat_fptype.view(torch.uint16).cpu().numpy()
        K_np = K_cat_fptype.view(torch.uint16).cpu().numpy()
        V_np = V_cat_fptype.view(torch.uint16).cpu().numpy()
        O_np = O_cat_fptype.view(torch.uint16).cpu().numpy()

    # quick sanity prints
    print("Q_cat_fptype:", tuple(Q_cat_fptype.shape), Q_cat_fptype.dtype)
    print("K_cat_fptype:", tuple(K_cat_fptype.shape), K_cat_fptype.dtype)
    print("V_cat_fptype:", tuple(V_cat_fptype.shape), V_cat_fptype.dtype)
    print("O_cat_fptype:", tuple(O_cat_fptype.shape), O_cat_fptype.dtype)

    print("Q_np:", Q_np.shape, Q_np.dtype)
    print("K_np:", K_np.shape, K_np.dtype)
    print("V_np:", V_np.shape, V_np.dtype)
    print("O_np:", O_np.shape, O_np.dtype)

    O_empty = np.zeros(O_np.shape, dtype=O_np.dtype)
    O_golden = O_np

    return Q_np, K_np, V_np, O_empty, O_golden


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate C header files from a ATTN preload file and Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("elf_path", type=str, help="Path of preload elf file")
    parser.add_argument("header_path", type=str, help="Path to ATTN preload C header file")
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

    #instanciate arch and attn configurations
    attn = FlatAttetion()
    arch = FlexClusterArch()

    #basic parameters
    dtype               = attn.dtype
    kv_sequence_length  = attn.kv_sequence_length
    q_sequence_length   = attn.q_sequence_length * attn.speculative_length * int(attn.num_head // attn.num_head_group)
    head_dimemsion      = attn.head_dimemsion
    effective_head      = attn.num_head_group
    batch_size          = attn.batch_size

    #generate numpy array
    Q_np, K_np, V_np, O_empty, O_golden = gen_attention_preload_numpy_arrays(batch_size, effective_head, head_dimemsion, q_sequence_length, kv_sequence_length, dtype)
    Q_addr = arch.hbm_start_base
    O_eaddr = Q_addr + Q_np.nbytes
    O_gaddr = O_eaddr + O_empty.nbytes
    K_addr = arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x
    V_addr = K_addr + K_np.nbytes
    print(f"Q_addr = {Q_addr: #x}")
    print(f"O_eaddr = {O_eaddr: #x}")
    print(f"O_gaddr = {O_gaddr: #x}")
    print(f"K_addr = {K_addr: #x}")
    print(f"V_addr = {V_addr: #x}")

    #generate preload elf
    pld.make_preload_elf(args.elf_path, 
        [Q_np,    K_np,   V_np,   O_empty, O_golden],
        [Q_addr,  K_addr, V_addr, O_eaddr, O_gaddr])

    #generate preload header file
    with open(args.header_path, 'w') as file:
        file.write('#ifndef _ATTN_PRELOAD_H_\n')
        file.write('#define _ATTN_PRELOAD_H_\n\n')
        file.write(f'#define {"Q_addr".upper()} ((uint64_t){Q_addr: #x})\n')
        file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
        file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
        file.write(f'#define {"K_addr".upper()} ((uint64_t){K_addr: #x})\n')
        file.write(f'#define {"V_addr".upper()} ((uint64_t){V_addr: #x})\n')
        file.write('\n#endif // _ATTN_PRELOAD_H_\n')

    print(f'Header file "{args.header_path}" generated successfully.')

