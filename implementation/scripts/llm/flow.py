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
import utils.softhier_engine as engine
import utils.console_visualization as cv
import llm.normal_llm_plan as normal_llm
import llm.deepseek_plan as deepseek

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

def print_dict_as_table(data):
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

def shape_kernel_flow(kernel_flow, west_hbm_plan, south_hbm_plan):
    for kernel_name in kernel_flow:
        for k, v in kernel_flow[kernel_name].items():
            if k != "type" and k != "cfg":
                direction = v["on"]
                if   direction == 'west':
                    kernel_flow[kernel_name][k]['shape'] = west_hbm_plan[v["name"]]['shape']
                elif direction == 'south':
                    kernel_flow[kernel_name][k]['shape'] = south_hbm_plan[v["name"]]['shape']
                else:
                    raise RuntimeError(f"HBM Direction {direction} currently not supported")
                    pass
                pass
            pass
        pass
    pass

def pack_data(kernel, west_hbm_plan, south_hbm_plan):
    data_dist = {}
    for k, v in kernel.items():
        if k != "type" and k != "cfg":
            direction = v["on"]
            if   direction == 'west':
                data_dist[k] = west_hbm_plan[v["name"]]
            elif direction == 'south':
                data_dist[k] = south_hbm_plan[v["name"]]
            else:
                raise RuntimeError(f"HBM Direction {direction} currently not supported")
                pass
            pass
        pass
    return data_dist
    pass

def softhier_launch(chip, launch_name, kernel_flow, west_hbm_plan, south_hbm_plan, info = {}):
    info['kernel_flow'] = kernel_flow
    info['west_hbm_plan'] = west_hbm_plan
    info['south_hbm_plan'] = south_hbm_plan
    cv.show_key_flow(kernel_flow)
    chip.register_workload(launch_name, info)
    view_onnx.create_onnx_graph(kernel_flow, west_hbm_plan, south_hbm_plan, chip.output_folder_info / "workload.onnx")
    pbar = tqdm(total=len(kernel_flow), desc=f"[{launch_name}]")
    kernel_results = {}
    for name, kernel in kernel_flow.items():
        desc = f"[{launch_name}][{name}]"
        pbar.set_description(desc)
        if   (kernel["type"] == 'norm'):
            #Normalization
            data = pack_data(kernel, west_hbm_plan, south_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.norm(cfg, data, name)
            kernel_results[name] = res
        elif (kernel["type"] == 'gemm'):
            #GEMM
            data = pack_data(kernel, west_hbm_plan, south_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.gemm_auto(cfg, data, name)
            kernel_results[name] = res
        elif (kernel["type"] == 'flat_attn'):
            #FlatAttention
            data = pack_data(kernel, west_hbm_plan, south_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.flat_attn_auto(cfg, data, name)
            kernel_results[name] = res
        elif (kernel["type"] == 'rope'):
            #RoPE
            data = pack_data(kernel, west_hbm_plan, south_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.rope(cfg, data, name)
            kernel_results[name] = res
        elif (kernel["type"] == 'acti'):
            #Activation
            data = pack_data(kernel, west_hbm_plan, south_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.acti(cfg, data, name)
            kernel_results[name] = res
        elif (kernel["type"] == 'addi'):
            #Addition
            data = pack_data(kernel, west_hbm_plan, south_hbm_plan)
            cfg = kernel["cfg"]
            res = chip.addi(cfg, data, name)
            kernel_results[name] = res
        else:
            kernel_type = kernel["type"]
            raise RuntimeError(f"Kernel {kernel_type} currently not supported")
            pass
        pbar.update(1)
        pass
    pbar.close()
    chip.record_info({"Results" : kernel_results})
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
        kernel_flow, west_hbm_plan, south_hbm_plan = normal_llm.normal_llm_prefill_layer_plan(llm, work, arch)
        pass

    if work.decode_enabled and llm.attention_type == 'MLA'and llm.ffn_type == 'MoE':
        kernel_flow, west_hbm_plan, south_hbm_plan = deepseek.deepseek_decode_layer_plan(llm, work, arch)
        print("")
        print("")
        print("[green][West HBM Plan][/green]")
        for k, v in west_hbm_plan.items():
            print(f"[yellow]| {k}:[/yellow]")
            print_dict_as_table(v)
            pass
        print("")
        print("")
        print("[green][South HBM Plan][/green]")
        for k, v in south_hbm_plan.items():
            print(f"[yellow]| {k}:[/yellow]")
            print_dict_as_table(v)
            pass
        print("")
        print("")
        shape_kernel_flow(kernel_flow, west_hbm_plan, south_hbm_plan)
        print("[green][Kernel Flow][/green]")
        for k, v in kernel_flow.items():
            print(f"[yellow]{k}:[/yellow]")
            print_dict_as_table(v)
            print(f"[yellow]|[/yellow]")
            print(f"[yellow]v[/yellow]")
            pass
        print(f"[yellow]End[/yellow]")
        print(f"[green][West HBM Occupancy Breakdown][/green]")
        cv.show_breakdown(west_hbm_plan, metric='size', unit='KiB', scale_div=1024)
        print(f"[green][South HBM Occupancy Breakdown][/green]")
        cv.show_breakdown(south_hbm_plan, metric='size', unit='KiB', scale_div=1024)
        cv.show_key_flow(kernel_flow)
        info['kernel_flow'] = kernel_flow
        info['west_hbm_plan'] = west_hbm_plan
        info['south_hbm_plan'] = south_hbm_plan
        chip.register_workload(f"{llm.model_name} decode phase", info)
        view_onnx.create_onnx_graph(kernel_flow, west_hbm_plan, south_hbm_plan, chip.output_folder_info / "workload.onnx")
        pass

    # Results = softhier_launch(chip, f"{llm.model_name} Prefill Sequence {work.prefill_input_token}", kernel_flow, west_hbm_plan, south_hbm_plan, info)

    # print(f"[green][West HBM Occupancy Breakdown][/green]")
    # cv.show_breakdown(west_hbm_plan, metric='size', unit='KiB', scale_div=1024)

    # print(f"[green][South HBM Occupancy Breakdown][/green]")
    # cv.show_breakdown(south_hbm_plan, metric='size', unit='KiB', scale_div=1024)

    # print(f"[green][Kernel Runtime Breakdown][/green]")
    # cv.show_breakdown(Results, metric='runtime', unit='us', scale_div=1000)

    pass

if __name__ == '__main__':
    flow()