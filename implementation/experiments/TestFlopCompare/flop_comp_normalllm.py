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

    #llm initialization
    llm = Model()
    llm.model_name                        = "Qwen-7B-Chat"
    llm.dtype                             = 'fp16'
    llm.num_layers                        = 32
    llm.embeded_length                    = 4096
    llm.max_sequence_length               = 8192
    llm.attention_type                    = 'MHA'
    llm.num_heads                         = 32
    llm.head_dimension                    = 128
    llm.qk_rope_enable                    = 1
    llm.head_groups                       = 8
    llm.ffn_type                          = 'MLP'
    llm.mlp_inter_dim                     = 22016
    llm.mlp_acti_algo                     = 'silu'
    llm.mlp_acti_bias_enable              = 0

    #work initialization
    prefill = Workload()
    prefill.prefill_enabled               = 1
    prefill.decode_enabled                = 0
    prefill.batch_size                    = 128
    prefill.numerical_check_enable        = 0
    prefill.prefill_input_token           = 128

    decode = Workload()
    decode.prefill_enabled                = 0
    decode.decode_enabled                 = 1
    decode.batch_size                     = 128
    decode.numerical_check_enable         = 0
    decode.decode_mode                    = 'speculative'
    decode.speculative_factor             = 2
    decode.speculative_ratio              = 0.75
    decode.kv_cache_length                = 126

    return arch, llm, prefill, decode
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
    chip = engine.SoftHier(softhier_root=args.softhier_root, kernel_root=args.kernel_root, output_root=args.output_root, tag="test_flop_comparision_normal_llm")

    # Test Parameter Initialization
    arch, qewn, prefill, decode = test_initialization()
    normal_llms = [qewn]

    # SoftHier Initialization
    chip.compile_hw(arch=arch, arch_path=args.arch_path)

    # Prefill
    sqs = [128, 256, 512, 1024, 2048, 4096, 8192, 16384]
    for llm in normal_llms:
        for sq in sqs:
            work = prefill
            # Configuration
            work.prefill_input_token = sq
            info = {"llm": llm, "work": work}

            # Generate flow
            kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan = normal_llm.normal_llm_prefill_layer_plan(llm, work, arch)
            info['kernel_flow'] = kernel_flow
            info['spaceA_hbm_plan'] = spaceA_hbm_plan
            info['spaceB_hbm_plan'] = spaceB_hbm_plan
            print_dict_as_table(work.__dict__)

            # Dry Run
            results = flow.softhier_launch(chip, f"Prefill {llm.model_name} sq{sq}", kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info=info, dry_run=True)
            flop_breakdown = normal_llm.gen_flop_breakdown(results)
            chip.record_info({"flop_breakdown" : flop_breakdown})
            print(f"[green][Flop Breakdown][/green]")
            cv.show_breakdown(flop_breakdown, metric='FLOP', unit='GFLOP', scale_div=1024 * 1024 * 1024)
            pass
        pass
    
    # Decode
    kvs = [128, 256, 512, 1024, 2048, 4096, 8192, 16384]
    for llm in normal_llms:
        for kv in kvs:
            work = decode
            # Configuration
            work.kv_cache_length = kv - work.speculative_factor
            info = {"llm": llm, "work": work}

            # Generate flow
            kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan = normal_llm.normal_llm_decode_layer_plan(llm, work, arch)
            info['kernel_flow'] = kernel_flow
            info['spaceA_hbm_plan'] = spaceA_hbm_plan
            info['spaceB_hbm_plan'] = spaceB_hbm_plan
            print_dict_as_table(work.__dict__)

            # Dry Run
            results = flow.softhier_launch(chip, f"Decode {llm.model_name} kv{kv}", kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info=info, dry_run=True)
            flop_breakdown = normal_llm.gen_flop_breakdown(results)
            chip.record_info({"flop_breakdown" : flop_breakdown})
            print(f"[green][Flop Breakdown][/green]")
            cv.show_breakdown(flop_breakdown, metric='FLOP', unit='GFLOP', scale_div=1024 * 1024 * 1024)
            pass
        pass
    pass

if __name__ == '__main__':
    test()
