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

    #llm initialization
    llm.model_name                  = "DeepSeekV3-671B"
    llm.dtype                       = 'fp8'
    llm.num_layers                  = 61
    llm.embeded_length              = 7168
    llm.max_sequence_length         = 8192
    llm.attention_type              = 'MLA'
    llm.num_heads                   = 128
    llm.head_dimension              = 128
    llm.q_lora_rank                 = 1536
    llm.kv_lora_rank                = 512
    llm.rope_head_dim               = 64
    llm.ffn_type                    = 'MoE'
    llm.moe_acti_algo               = 'silu'
    llm.moe_inter_dim               = 2048
    llm.n_routed_experts            = 256
    llm.n_shared_experts            = 1
    llm.n_activated_experts         = 8

    #work initialization
    work.prefill_enabled            = 0
    work.decode_enabled             = 1
    work.batch_size                 = 1
    work.numerical_check_enable     = 0
    work.decode_mode                = 'speculative'
    work.speculative_factor         = 4
    work.speculative_ratio          = 0.75
    work.kv_cache_length            = 1020

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
    chip = engine.SoftHier(softhier_root=args.softhier_root, kernel_root=args.kernel_root, output_root=args.output_root, tag="test_dsv3_fp8_decode_batch_scale_at_ep8")

    # Test Parameter Initialization
    arch, llm, work = test_initialization()

    # SoftHier Initialization
    chip.compile_hw(arch=arch, arch_path=args.arch_path)

    # Step1: Increase Batch Size
    bs  = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]
    for b in bs:
        # Default
        batch_size = b
        kv_cache_length = 4096
        expert_parallelsim = 8
        speculative_factor = 4

        # Configuration
        work.batch_size = batch_size
        work.speculative_factor = speculative_factor
        work.kv_cache_length = kv_cache_length - work.speculative_factor
        info = {"llm": llm, "work": work}

        # Generate flow
        kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan = deepseek.deepseek_layer_plan(llm, work, arch, EP=expert_parallelsim)
        info['kernel_flow'] = kernel_flow
        info['spaceA_hbm_plan'] = spaceA_hbm_plan
        info['spaceB_hbm_plan'] = spaceB_hbm_plan
        deepseek.hbm_plan_summary(spaceA_hbm_plan)
        deepseek.hbm_plan_summary(spaceB_hbm_plan)
        print_dict_as_table(work.__dict__)
        kernel_flow_simple = deepseek.kernel_flow_simplify(kernel_flow)

        # Dry Run
        flow.softhier_launch(chip, f"DRY-RUN b={batch_size} kv={kv_cache_length} ep={expert_parallelsim} sp{speculative_factor}", kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info=info, kernel_flow_simple=kernel_flow_simple, dry_run=True)

        # Full Run
        Results = flow.softhier_launch(chip, f"{llm.model_name} decode b{batch_size} kv{kv_cache_length} ep{expert_parallelsim} sp{speculative_factor}", kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info=info, kernel_flow_simple=kernel_flow_simple)
        print(f"[green][Kernel Runtime Breakdown][/green]")
        cv.show_breakdown(Results, metric='runtime', unit='us', scale_div=1000)
        pass
    pass

if __name__ == '__main__':
    test()
