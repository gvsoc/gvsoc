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
import utils.softhier_engine as engine

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

def softhier_launch(chip, launch_name, kernel_flow, west_hbm_plan, south_hbm_plan):
    pbar = tqdm(total=len(kernel_flow), desc=f"[{launch_name}]")
    for name, kernel in kernel_flow.items():
        pbar.set_description(f"[{launch_name}] [{name}]")
        if   (kernel["type"] == 'norm'):
            #Normalization
            data = pack_data(kernel, west_hbm_plan, south_hbm_plan)
            cfg = kernel["cfg"]
            chip.norm(cfg, data, name)
        elif (kernel["type"] == 'gemm'):
            #GEMM
            data = pack_data(kernel, west_hbm_plan, south_hbm_plan)
            cfg = kernel["cfg"]
            chip.gemm_auto(cfg, data, name)
        else:
            kernel_type = kernel["type"]
            raise RuntimeError(f"Kernel {kernel_type} currently not supported")
            pass
        pbar.update(1)
        pass
    pbar.close()
    pass

def llm_prefill_layer_plan(llm, work, arch):

    #Basic Settings
    elem_size                           = 1 if llm.dtype == 'fp8' else 2
    kernel_flow                         = {}
    west_hbm_plan                       = {}
    south_hbm_plan                      = {}
    west_hbm_addr                       = arch.hbm_start_base
    south_hbm_addr                      = arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x

    west_hbm_plan["input"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size



    #################################
    #       1. Normalization        #
    #################################
    west_hbm_plan["attn_norm"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

    attn_norm_cfg                       = RMSNorm()
    attn_norm_cfg.dtype                 = llm.dtype
    attn_norm_cfg.m_size                = work.batch_size * work.prefill_input_token
    attn_norm_cfg.n_size                = llm.embeded_length
    attn_norm_cfg.norm_numer            = work.numerical_check_enable

    kernel_flow["attn_norm"] = {
        "type"                          : "norm",
        "input"                         : {"on": "west",    "name": "input"},
        "output"                        : {"on": "west",    "name": "attn_norm"},
        "cfg"                           : attn_norm_cfg
    }


    #################################
    #       2. Q Projection         #
    #################################
    south_hbm_plan["q_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.num_heads * llm.head_dimension),
        "size"                          : llm.embeded_length * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * llm.num_heads * llm.head_dimension * elem_size

    west_hbm_plan["attn_q"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.num_heads,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size

    attn_q_proj                         = SummaGEMM()
    attn_q_proj.dtype                   = llm.dtype
    attn_q_proj.m_size                  = work.batch_size * work.prefill_input_token
    attn_q_proj.n_size                  = llm.num_heads * llm.head_dimension
    attn_q_proj.k_size                  = llm.embeded_length
    attn_q_proj.resha_x_from_enable     = 0
    attn_q_proj.resha_z_to_enable       = 1
    attn_q_proj.resha_z_to_m            = work.batch_size * work.prefill_input_token * llm.num_heads
    attn_q_proj.summa_numer             = work.numerical_check_enable

    kernel_flow["attn_q_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "west",    "name": "attn_norm"},
        "weight"                        : {"on": "south",   "name": "q_proj_weight"},
        "output"                        : {"on": "west",    "name": "attn_q"},
        "cfg"                           : attn_q_proj
    }



    #################################
    #       3. KV Projection        #
    #################################
    south_hbm_plan["k_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.num_heads * llm.head_dimension),
        "size"                          : llm.embeded_length * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * llm.num_heads * llm.head_dimension * elem_size

    south_hbm_plan["attn_k"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.num_heads,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size

    attn_k_proj                         = SummaGEMM()
    attn_k_proj.dtype                   = llm.dtype
    attn_k_proj.m_size                  = work.batch_size * work.prefill_input_token
    attn_k_proj.n_size                  = llm.num_heads * llm.head_dimension
    attn_k_proj.k_size                  = llm.embeded_length
    attn_k_proj.resha_x_from_enable     = 0
    attn_k_proj.resha_z_to_enable       = 1
    attn_k_proj.resha_z_to_m            = work.batch_size * work.prefill_input_token * llm.num_heads
    attn_k_proj.summa_numer             = work.numerical_check_enable

    kernel_flow["attn_k_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "west",    "name": "attn_norm"},
        "weight"                        : {"on": "south",   "name": "k_proj_weight"},
        "output"                        : {"on": "south",   "name": "attn_k"},
        "cfg"                           : attn_k_proj
    }


    south_hbm_plan["v_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.num_heads * llm.head_dimension),
        "size"                          : llm.embeded_length * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * llm.num_heads * llm.head_dimension * elem_size

    south_hbm_plan["attn_v"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.num_heads,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size

    attn_v_proj                         = SummaGEMM()
    attn_v_proj.dtype                   = llm.dtype
    attn_v_proj.m_size                  = work.batch_size * work.prefill_input_token
    attn_v_proj.n_size                  = llm.num_heads * llm.head_dimension
    attn_v_proj.k_size                  = llm.embeded_length
    attn_v_proj.resha_x_from_enable     = 0
    attn_v_proj.resha_z_to_enable       = 1
    attn_v_proj.resha_z_to_m            = work.batch_size * work.prefill_input_token * llm.num_heads
    attn_v_proj.summa_numer             = work.numerical_check_enable

    kernel_flow["attn_v_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "west",    "name": "attn_norm"},
        "weight"                        : {"on": "south",   "name": "v_proj_weight"},
        "output"                        : {"on": "south",   "name": "attn_v"},
        "cfg"                           : attn_v_proj
    }


    #########################
    #       4. QK RoPE      #
    #########################

    #5. Attention

    #6. O Projection

    #7. ResNet Addition

    #8. Normalization

    #9. FFN Up Projection

    #10. FFN Gate Projection

    #11. FFN Activation

    #12. FFN Down Projection

    #13. ResNet Addtion
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
    print("[green][Kernel Flow][/green]")
    for k, v in kernel_flow.items():
        print(f"[yellow]{k}:[/yellow]")
        print_dict_as_table(v)
        print(f"[yellow]|[/yellow]")
        print(f"[yellow]v[/yellow]")
        pass
    print(f"[yellow]End[/yellow]")
    
    return kernel_flow, west_hbm_plan, south_hbm_plan
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
    chip = engine.SoftHier(softhier_root=args.softhier_root, kernel_root=args.kernel_root, output_root=args.output_root)
    arch = FlexClusterArch()
    llm = Model()
    work = Workload()

    # SoftHier Initialization
    chip.compile_hw(arch=arch, arch_path=args.arch_path)

    # LLM Layer Plan
    if work.prefill_enabled and llm.attention_type == 'MHA' and llm.ffn_type == 'MLP':
        kernel_flow, west_hbm_plan, south_hbm_plan = llm_prefill_layer_plan(llm, work, arch)
        # data = pack_data(kernel_flow["attn_k_proj"], west_hbm_plan, south_hbm_plan)
        # print_dict_as_table(data)
        softhier_launch(chip, f"{llm.model_name} Prefill Layer", kernel_flow, west_hbm_plan, south_hbm_plan)
        pass

    pass

if __name__ == '__main__':
    flow()