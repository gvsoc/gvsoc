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

import os
import io
import re
import sys
import torch
import shutil
import argparse
import numpy as np
import importlib.util
from tqdm import tqdm
from rich import print
import llm.flow as flow
from tabulate import tabulate
import utils.view_onnx as view_onnx
import llm.deepseek_plan as deepseek
import utils.softhier_engine as engine
import utils.console_visualization as cv
import llm.normal_llm_plan as normal_llm

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

def print_dict_as_table(data, to_hex = False):
    formatted = data
    if to_hex:
        formatted = {k: (f"0x{v:X}" if isinstance(v, int) else v) for k, v in data.items()}
    print(tabulate(formatted.items(), headers=["Key", "Value"]))
    pass


def align_addr(addr, align=0x10000):
    """
    Aligns the given address up to the nearest multiple of `align`.

    Args:
        addr (int): The address (integer) to align.
        align (int): The alignment boundary (default: 0x10000).

    Returns:
        int: The aligned address.
    """
    return (addr + align - 1) & ~(align - 1)


def total_size(hbm_plan):
    size = 0
    for k, v in hbm_plan.items():
        size += v['size']
        pass
    return size
    pass

def reoffset(hbm_plan, offest):
    for k in hbm_plan:
        hbm_plan[k]['addr'] += offest
        pass
    return hbm_plan
    pass

def reoffset_hbm_plans(arch, spaceA_hbm_plan, spaceB_hbm_plan):
    #1. Count the Number of HBM edges
    edges_count = sum(1 for x in arch.hbm_chan_placement if x != 0)
    assert(edges_count <= 2 and edges_count > 0), f"[Arch Error] we can not plan tensor with {edges_count} edges HBM: {arch.hbm_chan_placement}"
    edges_idx = np.nonzero(np.array(arch.hbm_chan_placement))[0]

    addr_start = [
        arch.hbm_start_base,
        arch.hbm_start_base + arch.hbm_node_addr_space * arch.num_cluster_y,
        arch.hbm_start_base + arch.hbm_node_addr_space * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x,
        arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x
    ]

    #2. if only one edge
    if edges_count == 1 :
        edges_addr = addr_start[edges_idx[0]]
        sizeA = total_size(spaceA_hbm_plan)
        spaceA_start = edges_addr
        spaceB_start = align_addr(spaceA_start + sizeA, align=0x100000)
        spaceA_hbm_plan = reoffset(spaceA_hbm_plan, spaceA_start)
        spaceB_hbm_plan = reoffset(spaceB_hbm_plan, spaceB_start)
        pass

    #3. if two edges
    if edges_count == 2:
        spaceA_start = addr_start[edges_idx[0]]
        spaceB_start = addr_start[edges_idx[1]]
        spaceA_hbm_plan = reoffset(spaceA_hbm_plan, spaceA_start)
        spaceB_hbm_plan = reoffset(spaceB_hbm_plan, spaceB_start)
        pass

    return spaceA_hbm_plan, spaceB_hbm_plan
    pass

def tmla_plan(dtype, batch_size, speculative, kv_cache, arch):
    #Basic Settings
    elem_size                           = 1 if dtype == 'fp8' else 2
    kernel_flow                         = {}
    spaceA_hbm_plan                     = {}
    spaceB_hbm_plan                     = {}
    spaceA_hbm_addr                     = 0
    spaceB_hbm_addr                     = 0
    sequence_length                     = speculative
    num_heads                           = 128
    kv_lora_rank                        = 512
    rope_head_dim                       = 64

    spaceA_hbm_plan["attn_qn"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(batch_size,  sequence_length,  num_heads,  kv_lora_rank),
        "shape"                         :(batch_size * sequence_length,  num_heads * kv_lora_rank),
        "size"                          : batch_size * sequence_length * num_heads * kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += batch_size * sequence_length * num_heads * kv_lora_rank * elem_size

    spaceA_hbm_plan["attn_qr"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(batch_size,  sequence_length,  num_heads,  rope_head_dim),
        "shape"                         :(batch_size * sequence_length,  num_heads * rope_head_dim),
        "size"                          : batch_size * sequence_length * num_heads * rope_head_dim * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += batch_size * sequence_length * num_heads * rope_head_dim * elem_size

    spaceB_hbm_plan["cn_caches"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(batch_size,  kv_cache,  kv_lora_rank),
        "shape"                         :(batch_size * kv_cache,  kv_lora_rank),
        "size"                          : batch_size * kv_cache * kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += batch_size * kv_cache * kv_lora_rank * elem_size

    spaceB_hbm_plan["cr_caches"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(batch_size,  kv_cache,  rope_head_dim),
        "shape"                         :(batch_size * kv_cache,  rope_head_dim),
        "size"                          : batch_size * kv_cache * rope_head_dim * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += batch_size * kv_cache * rope_head_dim * elem_size

    spaceA_hbm_plan["attn_o"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(batch_size,  sequence_length,  num_heads,  kv_lora_rank),
        "shape"                         :(batch_size * sequence_length,  num_heads * kv_lora_rank),
        "size"                          : batch_size * sequence_length * num_heads * kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += batch_size * sequence_length * num_heads * kv_lora_rank * elem_size

    attn_flatmla                        = FlatMLA()
    attn_flatmla.dtype                  = dtype
    attn_flatmla.kv_sequence_length     = kv_cache
    attn_flatmla.q_sequence_length      = sequence_length
    attn_flatmla.speculative_length     = sequence_length
    attn_flatmla.nope_head_dim          = kv_lora_rank
    attn_flatmla.rope_head_dim          = rope_head_dim
    attn_flatmla.num_head               = num_heads
    attn_flatmla.batch_size             = batch_size
    attn_flatmla.flatten_numer          = 0
    attn_flatmla.use_flash_attn         = 0

    kernel_flow["attn_flatmla"] = {
        "type"                          : "flatmla",
        "qn"                            : {"on": "spaceA",   "name": "attn_qn"},
        "qr"                            : {"on": "spaceA",   "name": "attn_qr"},
        "cn"                            : {"on": "spaceB",  "name": "cn_caches"},
        "cr"                            : {"on": "spaceB",  "name": "cr_caches"},
        "o"                             : {"on": "spaceA",   "name": "attn_o"},
        "cfg"                           : attn_flatmla
    }

    spaceA_hbm_plan, spaceB_hbm_plan = reoffset_hbm_plans(arch, spaceA_hbm_plan, spaceB_hbm_plan)

    return kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan
    pass


def test():
    # Set up argument parser
    parser = argparse.ArgumentParser(description="E2E LLM Flow.")
    parser.add_argument("softhier_root", type=str, help="Path to SoftHier Root Directory")
    parser.add_argument("kernel_root", type=str, help="Path to Kernel Root Directory")
    parser.add_argument("output_root", type=str, help="Path to Output Directory")
    parser.add_argument("arch_path", type=str, help="Path of SoftHier Architecture File")
    parser.add_argument(
        "module_paths",
        metavar="module_path",
        type=str,
        nargs="+",
        help="Paths to Python modules to import (absolute or relative)."
    )

    args = parser.parse_args()
    args.module_paths.append(args.arch_path)

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
    pass

    # Initialize Configuration
    chip = engine.SoftHier(softhier_root=args.softhier_root, kernel_root=args.kernel_root, output_root=args.output_root, tag="test_flatmla")

    # Test Parameter Initialization
    arch = FlexClusterArch()

    # SoftHier Initialization
    chip.compile_hw(arch=arch, arch_path=args.arch_path)

    # Variables
    tp = 'fp16'
    b  = 128
    sp = 2
    kv = 512

    # Generate flow
    info = {}
    kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan = tmla_plan(dtype = tp, batch_size = b, speculative = sp, kv_cache = kv, arch = arch)
    info['kernel_flow'] = kernel_flow
    info['spaceA_hbm_plan'] = spaceA_hbm_plan
    info['spaceB_hbm_plan'] = spaceB_hbm_plan

    # Dry Run
    flow.softhier_launch(chip, f"DRY-RUN b={b} kv={kv} sp={sp}", kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info=info, dry_run=True)

    # Full Run
    flow.softhier_launch(chip, f"FlatMLA decode b{b} kv{kv} sp{sp}", kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info=info)
    pass


if __name__ == '__main__':
    test()
