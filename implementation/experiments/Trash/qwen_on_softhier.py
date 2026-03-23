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

def test_initialization():
    arch = FlexClusterArch()
    llm = Model()
    work = Workload()

    #llm initialization — real Qwen-7B dimensions
    llm.model_name                 = "Qwen-7B-Chat"
    llm.dtype                      = 'fp16'
    llm.num_layers                 = 32
    llm.embeded_length             = 4096
    llm.max_sequence_length        = 8192
    llm.attention_type             = 'MHA'
    llm.num_heads                  = 32
    llm.head_dimension             = 128
    llm.qk_rope_enable             = 1
    llm.head_groups                = 8
    llm.ffn_type                   = 'MLP'
    llm.mlp_inter_dim              = 22016
    llm.mlp_acti_algo              = 'silu'
    llm.mlp_acti_bias_enable       = 0

    #work initialization — batch=1 auto-regressive decode (M=1, true SpMV)
    work.prefill_enabled            = 0
    work.decode_enabled             = 1
    work.batch_size                 = 1
    work.numerical_check_enable     = 0
    work.decode_mode                = 'auto-regressive'
    work.speculative_factor         = 1
    work.speculative_ratio          = 0.75
    work.kv_cache_length            = 1023  # total KV seq = 1 + 1023 = 1024 (divisible by attention tile)

    return arch, llm, work
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

    # Initialize Configuration
    normal_llm.init(args.module_paths)
    deepseek.init(args.module_paths)
    chip = engine.SoftHier(softhier_root=args.softhier_root, kernel_root=args.kernel_root, output_root=args.output_root, tag="qwen_on_softhier")

    # Test Parameter Initialization
    arch, llm, work = test_initialization()

    # SoftHier Initialization
    chip.compile_hw(arch=arch, arch_path=args.arch_path)

    # Generate flow
    kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan = normal_llm.normal_llm_decode_layer_plan(llm, work, arch)

    import copy

    # Fix RoPE contiguous_length for small M (batch=1)
    seq_len = work.speculative_factor if work.decode_mode == 'speculative' else 1
    m_actual = work.batch_size * seq_len
    for kname, kernel in kernel_flow.items():
        if kernel["type"] == "rope" and hasattr(kernel["cfg"], 'contiguous_length'):
            kernel["cfg"].contiguous_length = min(kernel["cfg"].contiguous_length, kernel["cfg"].m_size)

    # Save original GEMM configs before conversion
    gemm_kernels = {}
    for kname, kernel in kernel_flow.items():
        if kernel["type"] == "gemm":
            gemm_kernels[kname] = copy.deepcopy(kernel["cfg"])

    # ===== RUN 1: Dense GEMM (fp16) =====
    kernel_flow_dense = copy.deepcopy(kernel_flow)
    for kname, kernel in kernel_flow_dense.items():
        if kernel["type"] == "gemm":
            old_cfg = kernel["cfg"]
            new_cfg = SpatzGEMM()
            new_cfg.dtype    = 'fp16'
            new_cfg.m_size   = old_cfg.m_size
            new_cfg.n_size   = old_cfg.n_size
            new_cfg.k_size   = old_cfg.k_size
            new_cfg.summa_numer = 0
            kernel["cfg"]  = new_cfg
            kernel["type"] = "spatz_gemm"

    info = {"llm": llm, "work": work}
    info['kernel_flow'] = kernel_flow_dense
    info['spaceA_hbm_plan'] = spaceA_hbm_plan
    info['spaceB_hbm_plan'] = spaceB_hbm_plan

    print(f"\n[yellow]{'='*60}[/yellow]")
    print(f"[yellow]  RUN 1: Dense fp16 GEMM[/yellow]")
    print(f"[yellow]{'='*60}[/yellow]")
    Results_dense = flow.softhier_launch(chip, f"Dense fp16 GEMM", kernel_flow_dense, spaceA_hbm_plan, spaceB_hbm_plan, info=info)
    print(f"[green][Dense GEMM Runtime Breakdown][/green]")
    cv.show_breakdown(Results_dense, metric='runtime', unit='us', scale_div=1000)

    # ===== RUN 2: SpMM 2:4 (fp16) =====
    kernel_flow_sparse = copy.deepcopy(kernel_flow)
    for kname, kernel in kernel_flow_sparse.items():
        if kernel["type"] == "gemm":
            old_cfg = kernel["cfg"]
            new_cfg = SpatzSpMM()
            new_cfg.dtype    = 'fp16'
            new_cfg.m_size   = old_cfg.m_size
            new_cfg.n_size   = old_cfg.n_size
            new_cfg.k_size   = old_cfg.k_size
            new_cfg.n_sparse = 2
            new_cfg.m_sparse = 4
            new_cfg.summa_numer = 0
            kernel["cfg"]  = new_cfg
            kernel["type"] = "spatz_spmm"

    info['kernel_flow'] = kernel_flow_sparse

    print(f"\n[yellow]{'='*60}[/yellow]")
    print(f"[yellow]  RUN 2: SpMM 2:4 fp16[/yellow]")
    print(f"[yellow]{'='*60}[/yellow]")
    Results_sparse = flow.softhier_launch(chip, f"SpMM 2:4 fp16", kernel_flow_sparse, spaceA_hbm_plan, spaceB_hbm_plan, info=info)
    print(f"[green][SpMM 2:4 Runtime Breakdown][/green]")
    cv.show_breakdown(Results_sparse, metric='runtime', unit='us', scale_div=1000)

    # ===== COMPARISON =====
    print(f"\n[cyan]{'='*60}[/cyan]")
    print(f"[cyan]  COMPARISON: Dense vs SpMM 2:4[/cyan]")
    print(f"[cyan]{'='*60}[/cyan]")
    total_dense = sum(v.get('runtime', 0) for v in Results_dense.values())
    total_sparse = sum(v.get('runtime', 0) for v in Results_sparse.values())
    print(f"Dense  TOTAL: {total_dense/1000:.1f} us")
    print(f"SpMM   TOTAL: {total_sparse/1000:.1f} us")
    if total_sparse > 0:
        print(f"Speedup:      {total_dense/total_sparse:.2f}x")
    for kname in Results_dense:
        if kname in Results_sparse:
            d = Results_dense[kname].get('runtime', 0)
            s = Results_sparse[kname].get('runtime', 0)
            spd = f"{d/s:.2f}x" if s > 0 else "N/A"
            print(f"  {kname:20s}: {d/1000:8.1f} vs {s/1000:8.1f} us  ({spd})")

    # ===== RUN 3: SpMM 1:4 (fp16) =====
    kernel_flow_sparse14 = copy.deepcopy(kernel_flow)
    for kname, kernel in kernel_flow_sparse14.items():
        if kernel["type"] == "gemm":
            old_cfg = kernel["cfg"]
            new_cfg = SpatzSpMM()
            new_cfg.dtype    = 'fp16'
            new_cfg.m_size   = old_cfg.m_size
            new_cfg.n_size   = old_cfg.n_size
            new_cfg.k_size   = old_cfg.k_size
            new_cfg.n_sparse = 1
            new_cfg.m_sparse = 4
            new_cfg.summa_numer = 0
            kernel["cfg"]  = new_cfg
            kernel["type"] = "spatz_spmm"

    info['kernel_flow'] = kernel_flow_sparse14

    print(f"\n[yellow]{'='*60}[/yellow]")
    print(f"[yellow]  RUN 3: SpMM 1:4 fp16[/yellow]")
    print(f"[yellow]{'='*60}[/yellow]")
    Results_sparse14 = flow.softhier_launch(chip, f"SpMM 1:4 fp16", kernel_flow_sparse14, spaceA_hbm_plan, spaceB_hbm_plan, info=info)
    print(f"[green][SpMM 1:4 Runtime Breakdown][/green]")
    cv.show_breakdown(Results_sparse14, metric='runtime', unit='us', scale_div=1000)

    # ===== FINAL COMPARISON =====
    print(f"\n[cyan]{'='*60}[/cyan]")
    print(f"[cyan]  FINAL COMPARISON: Dense vs SpMM 2:4 vs SpMM 1:4[/cyan]")
    print(f"[cyan]{'='*60}[/cyan]")
    total_14 = sum(v.get('runtime', 0) for v in Results_sparse14.values())
    print(f"Dense    TOTAL: {total_dense/1000:.1f} us")
    print(f"SpMM 2:4 TOTAL: {total_sparse/1000:.1f} us  ({total_dense/total_sparse:.2f}x)")
    print(f"SpMM 1:4 TOTAL: {total_14/1000:.1f} us  ({total_dense/total_14:.2f}x)")
    print()
    for kname in Results_dense:
        d = Results_dense[kname].get('runtime', 0)
        s24 = Results_sparse.get(kname, {}).get('runtime', 0)
        s14 = Results_sparse14.get(kname, {}).get('runtime', 0)
        spd24 = f"{d/s24:.2f}x" if s24 > 0 else "N/A"
        spd14 = f"{d/s14:.2f}x" if s14 > 0 else "N/A"
        print(f"  {kname:20s}: {d/1000:7.1f} | {s24/1000:7.1f} ({spd24}) | {s14/1000:7.1f} ({spd14})")

    pass

if __name__ == '__main__':
    test()