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
from tabulate import tabulate
import utils.view_onnx as view_onnx
import llm.deepseek_plan as deepseek
import utils.softhier_engine as engine
import utils.console_visualization as cv
import llm.normal_llm_plan as normal_llm
import utils.analyze_results as ar

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

def shape_kernel_flow(kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan):
    for kernel_name in kernel_flow:
        for k, v in kernel_flow[kernel_name].items():
            if k != "type" and k != "cfg"  and k != "info":
                direction = v["on"]
                if   direction == 'spaceA':
                    kernel_flow[kernel_name][k]['shape'] = spaceA_hbm_plan[v["name"]]['shape']
                elif direction == 'spaceB':
                    kernel_flow[kernel_name][k]['shape'] = spaceB_hbm_plan[v["name"]]['shape']
                else:
                    raise RuntimeError(f"HBM Direction {direction} currently not supported")
                    pass
                pass
            pass
        pass
    pass

def pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan):
    data_dist = {}
    for k, v in kernel.items():
        if k != "type" and k != "cfg" and k != "repeat":
            if k == "info" and "merged_output" in v:
                direction = v["merged_output"]["on"]
                tensor_name = v["merged_output"]["name"]
                if   direction == 'spaceA':
                    data_dist["merged_output"] = spaceA_hbm_plan[tensor_name]
                elif direction == 'spaceB':
                    data_dist["merged_output"] = spaceB_hbm_plan[tensor_name]
                else:
                    raise RuntimeError(f"HBM Direction {direction} currently not supported")
                    pass
                pass

            if k == "info" and "merged_input" in v:
                direction = v["merged_input"]["on"]
                tensor_name = v["merged_input"]["name"]
                if   direction == 'spaceA':
                    data_dist["merged_input"] = spaceA_hbm_plan[tensor_name]
                elif direction == 'spaceB':
                    data_dist["merged_input"] = spaceB_hbm_plan[tensor_name]
                else:
                    raise RuntimeError(f"HBM Direction {direction} currently not supported")
                    pass
                pass

            if k != "info":
                direction = v["on"]
                tensor_name = v["name"]
                if   direction == 'spaceA':
                    data_dist[k] = spaceA_hbm_plan[tensor_name]
                elif direction == 'spaceB':
                    data_dist[k] = spaceB_hbm_plan[tensor_name]
                else:
                    raise RuntimeError(f"HBM Direction {direction} currently not supported")
                    pass
                pass
            pass
        pass
    return data_dist
    pass

def push_results(kernel_results, name, kernel, res):
    if res == None:
        return
        pass
    if "repeat" in kernel:
        res["runtime"] = res["runtime"] * kernel['repeat']
        kernel_results[f"{name}(x{kernel['repeat']})"] = res
    else:
        kernel_results[name] = res
        pass
    pass

def softhier_run_flow(chip, run_flow, spaceA_hbm_plan, spaceB_hbm_plan, run_name, dry_run=False):
    pbar = tqdm(total=len(run_flow), desc=f"[{run_name}]")
    kernel_results = {}
    for name, kernel in run_flow.items():
        desc = f"[{run_name}][{name}]"
        pbar.set_description(desc)
        if   (kernel["type"] == 'norm'):
            #Normalization
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.norm(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'gemm'):
            #GEMM
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.gemm_auto(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'flat_attn'):
            #FlatAttention
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.flat_attn_auto(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'rope'):
            #RoPE
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.rope(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'acti'):
            #Activation
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.acti(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'addi'):
            #Addition
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.addi(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'split_concat'):
            #Split and KVcace Concat
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.split_concat(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'flatmla'):
            #FlatMLA
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.flat_mla_auto(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'ofdp'):
            #MLA First O Down Porjection
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.mla_ofdp(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'moeg'):
            #MoE Gate TopK
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.moe_gate_topk(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'moed'):
            #MoE Token Dispatch
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.moe_dispatch(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        elif (kernel["type"] == 'moec'):
            #MoE Token Combine
            data = pack_data(kernel, spaceA_hbm_plan, spaceB_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.moe_combine(cfg, data, name, dry_run=dry_run)
            push_results(kernel_results, name, kernel, res)
        else:
            kernel_type = kernel["type"]
            raise RuntimeError(f"Kernel {kernel_type} currently not supported")
            pass
        pbar.update(1)
        pass
    pbar.close()
    return kernel_results

def softhier_launch(chip, launch_name, kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info = {}, kernel_flow_simple = None, dry_run=False):
    info['kernel_flow'] = kernel_flow
    info['spaceA_hbm_plan'] = spaceA_hbm_plan
    info['spaceB_hbm_plan'] = spaceB_hbm_plan
    if kernel_flow_simple != None:
        info['kernel_flow_simple'] = kernel_flow_simple
        cv.show_key_flow(kernel_flow_simple)
        run_flow = kernel_flow_simple
    else:
        cv.show_key_flow(kernel_flow)
        run_flow = kernel_flow
        pass
    chip.register_workload(launch_name, info)
    view_onnx.create_onnx_graph(kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, chip.output_folder_info / "workload.onnx")
    kernel_results = softhier_run_flow(chip, run_flow, spaceA_hbm_plan, spaceB_hbm_plan, launch_name, dry_run=dry_run)
    if kernel_results:
        chip.record_info({"Results" : kernel_results})
        ar.generate_polts(arch=chip.arch, results=kernel_results, save_root=chip.output_folder_info)
        pass
    return kernel_results
    pass



def flow():
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
    chip = engine.SoftHier(softhier_root=args.softhier_root, kernel_root=args.kernel_root, output_root=args.output_root)
    arch = FlexClusterArch()
    llm = Model()
    work = Workload()
    info = {"llm": llm, "work": work}

    # SoftHier Initialization
    chip.compile_hw(arch=arch, arch_path=args.arch_path)

    # LLM Layer Plan
    if work.prefill_enabled and llm.attention_type == 'MHA' and llm.ffn_type == 'MLP':
        kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan = normal_llm.normal_llm_prefill_layer_plan(llm, work, arch)
        pass

    if work.decode_enabled and llm.attention_type == 'MLA'and llm.ffn_type == 'MoE':
        kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan = deepseek.deepseek_decode_layer_plan(llm, work, arch, EP=1)
        info['kernel_flow'] = kernel_flow
        info['spaceA_hbm_plan'] = spaceA_hbm_plan
        info['spaceB_hbm_plan'] = spaceB_hbm_plan
        deepseek.hbm_plan_summary(spaceA_hbm_plan)
        deepseek.hbm_plan_summary(spaceB_hbm_plan)
        print_dict_as_table(work.__dict__)
        kernel_flow_simple = deepseek.kernel_flow_simplify(kernel_flow)
        softhier_launch(chip, f"{llm.model_name} decode DRY-RUN", kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info=info, kernel_flow_simple=kernel_flow_simple, dry_run=True)
        Results = softhier_launch(chip, f"{llm.model_name} decode", kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, info=info, kernel_flow_simple=kernel_flow_simple)
        print(f"[green][Kernel Runtime Breakdown][/green]")
        cv.show_breakdown(Results, metric='runtime', unit='us', scale_div=1000)
        pass

    pass

if __name__ == '__main__':
    flow()