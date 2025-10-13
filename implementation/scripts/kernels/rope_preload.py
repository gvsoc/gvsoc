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


def rope_cos_table(max_seq_len, d, dtype, base=10000):
    """
    Generate a table of cos(m * theta) for RoPE embeddings.

    Args:
        max_seq_len (int): number of positions (sequence length)
        d (int): embedding dimension (must be even)
        base (float): base constant for frequency scaling (default=10000)

    Returns:
        torch.Tensor: [max_seq_len, d] tensor containing cos(m * theta)
    """
    # frequencies
    i = torch.arange(0, d // 2, dtype=torch.float32)
    theta = base ** (-2 * i / d)  # [d/2]

    # positions
    m = torch.arange(0, max_seq_len, dtype=torch.float32).unsqueeze(1)  # [seq_len, 1]

    # outer product → m * theta → [seq_len, d/2]
    angles = m * theta  # broadcast multiplication

    # now get cos and sin
    cos_table = torch.cos(angles).to(dtype)
    sin_table = torch.sin(angles).to(dtype)

    return cos_table, sin_table

def rope_position_list(num: int, contiguous_length: int, maxn: int, *, dtype=torch.int32, seed: int | None = None) -> torch.Tensor:
    """
    Create a 1D tensor made of `num` fragments, each length `contiguous_length`.
    Each fragment is consecutive numbers [s, s+1, ..., s+contiguous_length-1] where s ~ Uniform{0, ..., maxn}.
    The total length is num * contiguous_length.

    Args:
        num: number of fragments
        contiguous_length: length of each fragment
        maxn: inclusive upper bound for the start of each fragment
        dtype: tensor dtype (default: torch.long)
        seed: optional RNG seed for reproducibility

    Returns:
        torch.Tensor of shape (num * contiguous_length,)
    """
    if num <= 0 or contiguous_length <= 0:
        raise ValueError("`num` and `contiguous_length` must be positive integers.")
    if maxn < 0:
        raise ValueError("`maxn` must be >= 0.")

    # Optional reproducibility
    g = torch.Generator(device='cpu')
    if seed is not None:
        g.manual_seed(seed)

    # Sample starts in [0, maxn], shape (num, 1)
    starts = torch.randint(0, maxn + 1, (num, 1), generator=g)

    # Build one fragment [0, 1, ..., contiguous_length-1], shape (1, contiguous_length)
    base = torch.arange(contiguous_length, dtype=dtype).unsqueeze(0)

    # Broadcast add -> (num, contiguous_length), then flatten -> (num*contiguous_length,)
    out = (starts.to(dtype) + base).reshape(-1)

    return out

def gen_rope_preload_numpy_arrays(rope):
    # ---- params you can change ----
    seed = 0
    # --------------------------------

    torch.manual_seed(seed)

    # pick device; fp8 compute is limited, so we do compute in f16/f32 and store in fp8
    device = torch.device("cpu")

    # make sure FP8 (e5m2) is available in this PyTorch
    if not hasattr(torch, "float8_e5m2"):
        raise RuntimeError("This PyTorch build does not expose torch.float8_e5m2.")

    fptype = torch.float8_e5m2 if rope.dtype == "fp8" else torch.float16

    # helper: create random fptype tensors by sampling in f16 then casting to fptype for storage
    def rand_fptype(shape, device):
        # sample in a safe range to reduce saturation when casting to fptype
        # normal(0, 0.5) tends to survive e5m2 rounding reasonably well
        x = torch.randn(shape, device=device, dtype=torch.float32) * 0.1 + 0.0625
        return x.to(fptype)

    compute_dtype = torch.float16

    # RoPE
    ## Cos and Sin table
    C_fptype, S_fptype = rope_cos_table(rope.maximun_seqlen, rope.n_size, fptype)
    ## Generate the position list
    P_intype = rope_position_list(num=(rope.m_size // rope.contiguous_length), contiguous_length=rope.contiguous_length, maxn=(rope.maximun_seqlen - rope.contiguous_length), dtype=torch.int32, seed=seed)
    # print(f"Generated Position List = {P_intype}")
    ## Generate Input
    I_fptype = rand_fptype((rope.m_size,  rope.n_size), device)

    C_org = C_fptype.to(compute_dtype)
    S_org = S_fptype.to(compute_dtype)
    I = I_fptype.to(compute_dtype)
    C = C_org[P_intype]
    S = S_org[P_intype]
    A = I[:,  ::2]
    B = I[:, 1::2]
    AS = A * S
    BS = B * S
    AC = A * C
    BC = B * C
    P1 = AC - BS
    P2 = AS + BC
    O = torch.stack((P1, P2), dim=2).reshape(P1.size(0), -1)
    O_fptype = O.to(fptype)

    # dealing with different matrix view
    if rope.view_enable:
        I_fptype = torch.cat(I_fptype.split(rope.view_n, dim=1), dim=0)
        O_fptype = torch.cat(O_fptype.split(rope.view_n, dim=1), dim=0)
        pass

    # convert to NumPy: cast to float16 (NumPy may not support PyTorch fptype dtype)
    if fptype == torch.float8_e5m2:
        I_np = I_fptype.view(torch.uint8).cpu().numpy()
        O_np = O_fptype.view(torch.uint8).cpu().numpy()
        C_np = C_fptype.view(torch.uint8).cpu().numpy()
        S_np = S_fptype.view(torch.uint8).cpu().numpy()
    else:
        I_np = I_fptype.view(torch.uint16).cpu().numpy()
        O_np = O_fptype.view(torch.uint16).cpu().numpy()
        C_np = C_fptype.view(torch.uint16).cpu().numpy()
        S_np = S_fptype.view(torch.uint16).cpu().numpy()
    P_np = P_intype.cpu().numpy()

    # quick sanity prints
    print("I_fptype:", tuple(I_fptype.shape), I_fptype.dtype)
    print("O_fptype:", tuple(O_fptype.shape), O_fptype.dtype)
    print("C_fptype:", tuple(C_fptype.shape), C_fptype.dtype)
    print("S_fptype:", tuple(S_fptype.shape), S_fptype.dtype)
    print("P_intype:", tuple(P_intype.shape), P_intype.dtype)

    print("I_np:", I_np.shape, I_np.dtype)
    print("O_np:", O_np.shape, O_np.dtype)
    print("C_np:", C_np.shape, C_np.dtype)
    print("S_np:", S_np.shape, S_np.dtype)
    print("P_np:", P_np.shape, P_np.dtype)

    O_empty = np.zeros(O_np.shape, dtype=O_np.dtype)
    O_golden = O_np

    return I_np, C_np, S_np, P_np, O_empty, O_golden


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate C header files from a RoPE preload file and Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("elf_path", type=str, help="Path of preload elf file")
    parser.add_argument("header_path", type=str, help="Path to RoPE preload C header file")
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

    # Instanciate arch and rope configurations
    rope = RoPE()
    arch = FlexClusterArch()

    # Generate the preload
    I_np, C_np, S_np, P_np, O_empty, O_golden = gen_rope_preload_numpy_arrays(rope)
    I_addr = arch.hbm_start_base
    O_eaddr = I_addr + I_np.nbytes
    O_gaddr = O_eaddr + O_empty.nbytes
    C_addr = arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x
    S_addr = C_addr + C_np.nbytes
    P_addr = S_addr + S_np.nbytes
    print(f"I_addr = {I_addr: #x}")
    print(f"O_eaddr = {O_eaddr: #x}")
    print(f"O_gaddr = {O_gaddr: #x}")
    print(f"C_addr = {C_addr: #x}")
    print(f"S_addr = {S_addr: #x}")
    print(f"P_addr = {P_addr: #x}")

    #generate preload elf
    if rope.rope_numer == 1:
        pld.make_preload_elf(args.elf_path, 
            [I_np,    C_np,    S_np,    P_np,    O_empty,  O_golden],
            [I_addr,  C_addr,  S_addr,  P_addr,  O_eaddr,  O_gaddr])
    else:
        dumnp = np.array([1,1,1,1,1])
        pld.make_preload_elf(args.elf_path, 
            [dumnp,   dumnp,   dumnp,   P_np,    dumnp,    dumnp],
            [I_addr,  C_addr,  S_addr,  P_addr,  O_eaddr,  O_gaddr])
        pass

    #generate preload header file
    with open(args.header_path, 'w') as file:
        file.write('#ifndef _ROPE_PRELOAD_H_\n')
        file.write('#define _ROPE_PRELOAD_H_\n\n')
        file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
        file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
        file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
        file.write(f'#define {"C_addr".upper()} ((uint64_t){C_addr: #x})\n')
        file.write(f'#define {"S_addr".upper()} ((uint64_t){S_addr: #x})\n')
        file.write(f'#define {"P_addr".upper()} ((uint64_t){P_addr: #x})\n')
        file.write('\n#endif // _ROPE_PRELOAD_H_\n')

    print(f'Header file "{args.header_path}" generated successfully.')
