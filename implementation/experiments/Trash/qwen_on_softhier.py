#
# Copyright (C) 2025 ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Author: Chi Zhang <chizhang@ethz.ch>
# Modified: Bowen Wang, ETH Zurich (Spatz/Ventaglio integration)

import os
import io
import re
import sys
import copy
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
    module_name = os.path.splitext(os.path.basename(module_path))[0]
    spec = importlib.util.spec_from_file_location(module_name, module_path)
    if spec is None:
        raise ImportError(f"Cannot find a module at path: {module_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    globals().update(vars(module))
    return module

def print_dict_as_table(data, to_hex = False):
    formatted = data
    if to_hex:
        formatted = {k: (f"0x{v:X}" if isinstance(v, int) else v) for k, v in data.items()}
    print(tabulate(formatted.items(), headers=["Key", "Value"]))

def run_3way_comparison(chip, kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info, label):
    """Run Dense, SpMM 2:4, SpMM 1:4 and print comparison."""

    # Fix RoPE contiguous_length for small M
    for kname, kernel in kernel_flow.items():
        if kernel["type"] == "rope" and hasattr(kernel["cfg"], 'contiguous_length'):
            kernel["cfg"].contiguous_length = min(kernel["cfg"].contiguous_length, kernel["cfg"].m_size)

    all_results = {}

    configs = [
        ("Dense fp16", "spatz_gemm", lambda: SpatzGEMM(), None, None),
        ("SpMM 2:4",   "spatz_spmm", lambda: SpatzSpMM(), 2, 4),
        ("SpMM 1:4",   "spatz_spmm", lambda: SpatzSpMM(), 1, 4),
    ]

    for cfg_label, ktype, cfg_factory, n_sp, m_sp in configs:
        kf = copy.deepcopy(kernel_flow)
        for kname, kernel in kf.items():
            if kernel["type"] == "gemm":
                old_cfg = kernel["cfg"]
                new_cfg = cfg_factory()
                new_cfg.dtype = 'fp16'
                new_cfg.m_size = old_cfg.m_size
                new_cfg.n_size = old_cfg.n_size
                new_cfg.k_size = old_cfg.k_size
                new_cfg.summa_numer = 0
                if n_sp is not None:
                    new_cfg.n_sparse = n_sp
                    new_cfg.m_sparse = m_sp
                kernel["cfg"] = new_cfg
                kernel["type"] = ktype

        info_copy = copy.deepcopy(info)
        info_copy['kernel_flow'] = kf

        run_name = f"{label} {cfg_label}"
        print(f"\n[yellow]  Running: {run_name}[/yellow]")
        results = flow.softhier_launch(chip, run_name, kf, spaceA_hbm_plan, spaceB_hbm_plan, info=info_copy)
        all_results[cfg_label] = results

    # Print comparison
    d = all_results["Dense fp16"]
    s24 = all_results["SpMM 2:4"]
    s14 = all_results["SpMM 1:4"]

    td = sum(v.get('runtime', 0) for v in d.values())
    t24 = sum(v.get('runtime', 0) for v in s24.values())
    t14 = sum(v.get('runtime', 0) for v in s14.values())

    print(f"\n[cyan]{'='*80}[/cyan]")
    print(f"[cyan]  {label}: Dense vs SpMM 2:4 vs SpMM 1:4[/cyan]")
    print(f"[cyan]{'='*80}[/cyan]")
    print(f"  Dense:    {td/1000:.1f} us")
    print(f"  SpMM 2:4: {t24/1000:.1f} us ({td/t24:.2f}x)" if t24 > 0 else "  SpMM 2:4: N/A")
    print(f"  SpMM 1:4: {t14/1000:.1f} us ({td/t14:.2f}x)" if t14 > 0 else "  SpMM 1:4: N/A")

    for kname in d:
        dv = d[kname].get('runtime', 0)
        sv24 = s24.get(kname, {}).get('runtime', 0)
        sv14 = s14.get(kname, {}).get('runtime', 0)
        if dv > 0 and 'proj' in kname:
            spd24 = f"{dv/sv24:.2f}x" if sv24 > 0 else "N/A"
            spd14 = f"{dv/sv14:.2f}x" if sv14 > 0 else "N/A"
            print(f"    {kname:20s}: {dv/1000:9.1f} | {sv24/1000:9.1f} ({spd24}) | {sv14/1000:9.1f} ({spd14})")

    return td, t24, t14

def test():
    parser = argparse.ArgumentParser(description="E2E LLM Flow.")
    parser.add_argument("softhier_root", type=str)
    parser.add_argument("kernel_root", type=str)
    parser.add_argument("output_root", type=str)
    parser.add_argument("arch_path", type=str)
    parser.add_argument("module_paths", metavar="module_path", type=str, nargs="+")

    args = parser.parse_args()
    args.module_paths.append(args.arch_path)

    for module_path in args.module_paths:
        absolute_path = os.path.abspath(module_path)
        if not os.path.isfile(absolute_path):
            continue
        try:
            module = import_module_from_path(absolute_path)
        except Exception as e:
            print(f"Failed to import {absolute_path}: {e}")

    normal_llm.init(args.module_paths)
    deepseek.init(args.module_paths)
    chip = engine.SoftHier(softhier_root=args.softhier_root, kernel_root=args.kernel_root,
                           output_root=args.output_root, tag="qwen_on_softhier")

    arch = FlexClusterArch()
    llm = Model()
    work = Workload()

    # Qwen-7B model
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
    work.numerical_check_enable     = 0

    # Compile HW once
    chip.compile_hw(arch=arch, arch_path=args.arch_path)

    # Collect all results for final summary
    summary = []

    # ===== TARGETED BENCHMARKS (2-core Spatz) =====
    # 1. Prefill 128 tokens (M=128): large M, 2-core benefits, near-theoretical
    # 2. Decode batch=1 (M=1): can't split M=1 across cores — shows limitation
    # 3. Decode batch=16 speculative (M=64): moderate M, 2-core benefits

    # Prefill
    for token_len in [128]:
        work.prefill_enabled        = 1
        work.decode_enabled         = 0
        work.batch_size             = 1
        work.prefill_input_token    = token_len

        label = f"Prefill {token_len} tokens (M={token_len}, 2-core)"
        print(f"\n[green]{'#'*80}[/green]")
        print(f"[green]  {label}[/green]")
        print(f"[green]{'#'*80}[/green]")

        kernel_flow, spaceA, spaceB = normal_llm.normal_llm_prefill_layer_plan(llm, work, arch)
        info = {"llm": llm, "work": work}
        td, t24, t14 = run_3way_comparison(chip, kernel_flow, spaceA, spaceB, info, label)
        summary.append((label, td, t24, t14))

    # Decode
    for batch, spec, mode in [(1, 1, 'auto-regressive'), (16, 4, 'speculative')]:
        work.prefill_enabled        = 0
        work.decode_enabled         = 1
        work.batch_size             = batch
        work.decode_mode            = mode
        work.speculative_factor     = spec
        work.speculative_ratio      = 0.75
        work.kv_cache_length        = 1023

        m_val = batch * spec
        label = f"Decode batch={batch} (M={m_val}, 2-core)"
        print(f"\n[green]{'#'*80}[/green]")
        print(f"[green]  {label}[/green]")
        print(f"[green]{'#'*80}[/green]")

        kernel_flow, spaceA, spaceB = normal_llm.normal_llm_decode_layer_plan(llm, work, arch)
        info = {"llm": llm, "work": work}
        td, t24, t14 = run_3way_comparison(chip, kernel_flow, spaceA, spaceB, info, label)
        summary.append((label, td, t24, t14))

    # ===== FINAL SUMMARY =====
    print(f"\n[cyan]{'='*80}[/cyan]")
    print(f"[cyan]  FULL BENCHMARK SUMMARY (Qwen-7B fp16, 1 layer)[/cyan]")
    print(f"[cyan]{'='*80}[/cyan]")
    print(f"{'Config':<35s} | {'Dense':>10s} | {'2:4':>10s} | {'1:4':>10s} | {'2:4 spd':>7s} | {'1:4 spd':>7s}")
    print("-" * 90)
    for label, td, t24, t14 in summary:
        spd24 = f"{td/t24:.2f}x" if t24 > 0 else "N/A"
        spd14 = f"{td/t14:.2f}x" if t14 > 0 else "N/A"
        print(f"{label:<35s} | {td/1000:>8.1f}us | {t24/1000:>8.1f}us | {t14/1000:>8.1f}us | {spd24:>7s} | {spd14:>7s}")

if __name__ == '__main__':
    test()
