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
import copy
import torch
import shutil
import numpy as np
import importlib.util
from tqdm import tqdm
from rich import print
import utils.console_visualization as cv

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

def init(module_paths):
	# Process each provided module path
    for module_path in module_paths:
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

def gen_flop_breakdown(results):
    flop_mla_porj = 0
    flop_mla_core = 0
    flop_moe = 0
    for k,v in results.items():
        if 'attn' in k and 'proj' in k and 'FLOP' in v:
            flop_mla_porj += v['FLOP']
        if 'attn_flatmla' in k and 'FLOP' in v:
            flop_mla_core += v['FLOP']
        if 'moe' in k and 'FLOP' in v:
            flop_moe += v['FLOP']
        pass
    flop_breakdown = {
        'ATN_proj': {'FLOP': flop_mla_porj},
        'ATN_core': {'FLOP': flop_mla_core},
        'FFN':      {'FLOP': flop_moe},
    }
    return flop_breakdown
    pass

def gen_moe_gate_index(num_tokens, num_routed_experts, num_active_experts, moe_distribution = 'Fair'):
    if moe_distribution == 'Fair':
        x = torch.arange(num_tokens * num_active_experts) % num_routed_experts
        idx = torch.randperm(x.size(0))
        x_shuffled = x[idx]
        D = x.view(num_tokens, num_active_experts).to(torch.int32)
    elif moe_distribution == 'Identical':
        x = torch.arange(num_tokens * num_active_experts) % num_active_experts
        D = x.view(num_tokens, num_active_experts).to(torch.int32)
    else:
        raise RuntimeError(f"MoE Distribution {moe_distribution} currently not supported")
        pass
    P = torch.zeros(D.shape, dtype=D.dtype)
    SCORE = torch.zeros(num_routed_experts, dtype=torch.int32)
    for i in tqdm(range(D.shape[0]), desc="[Expert Positioning]"):
        for j in range(D.shape[1]):
            #1. get expert idx
            expert_idx = D[i, j]
            #2. get position
            pos = SCORE[expert_idx].clone()
            SCORE[expert_idx] += 1
            #3. store position
            P[i, j] = pos
            pass
        pass
    return D, P, SCORE
    pass

def kernel_flow_simplify(kernel_flow_in):
    kernel_flow = copy.deepcopy(kernel_flow_in)
    # Set Repeat for redundant kernels
    keys = []
    pattern = r"^moe_routed_\d+_up$"
    for k, v in kernel_flow.items():
        if re.match(pattern, k):
            keys.append(k)
            pass
        pass
    compare_list = [[kernel_flow[k]['cfg'].m_size, kernel_flow[k]['cfg'].n_size, kernel_flow[k]['cfg'].k_size] for k in keys]
    unique_list = []
    unique_id_list = []
    for i in range(len(compare_list)):
        obj = compare_list[i]
        if obj not in unique_list:
            unique_list.append(obj)
            unique_id_list.append(i)
    for i in unique_id_list:
        kernel_flow[keys[i]]["repeat"] = compare_list.count(compare_list[i])
        pass
    keys_to_delete = []
    for i in range(len(keys)):
        if i not in unique_id_list: keys_to_delete.append(keys[i])
        pass
    for k in keys_to_delete: del kernel_flow[k]

    keys = []
    pattern = r"^moe_routed_\d+_gate$"
    for k, v in kernel_flow.items():
        if re.match(pattern, k):
            keys.append(k)
            pass
        pass
    compare_list = [[kernel_flow[k]['cfg'].m_size, kernel_flow[k]['cfg'].n_size, kernel_flow[k]['cfg'].k_size] for k in keys]
    unique_list = []
    unique_id_list = []
    for i in range(len(compare_list)):
        obj = compare_list[i]
        if obj not in unique_list:
            unique_list.append(obj)
            unique_id_list.append(i)
    for i in unique_id_list:
        kernel_flow[keys[i]]["repeat"] = compare_list.count(compare_list[i])
        pass
    keys_to_delete = []
    for i in range(len(keys)):
        if i not in unique_id_list: keys_to_delete.append(keys[i])
        pass
    for k in keys_to_delete: del kernel_flow[k]

    keys = []
    pattern = r"^moe_routed_\d+_acti$"
    for k, v in kernel_flow.items():
        if re.match(pattern, k):
            keys.append(k)
            pass
        pass
    compare_list = [[kernel_flow[k]['cfg'].m_size, kernel_flow[k]['cfg'].n_size] for k in keys]
    unique_list = []
    unique_id_list = []
    for i in range(len(compare_list)):
        obj = compare_list[i]
        if obj not in unique_list:
            unique_list.append(obj)
            unique_id_list.append(i)
    for i in unique_id_list:
        kernel_flow[keys[i]]["repeat"] = compare_list.count(compare_list[i])
        pass
    keys_to_delete = []
    for i in range(len(keys)):
        if i not in unique_id_list: keys_to_delete.append(keys[i])
        pass
    for k in keys_to_delete: del kernel_flow[k]

    keys = []
    pattern = r"^moe_routed_\d+_down$"
    for k, v in kernel_flow.items():
        if re.match(pattern, k):
            keys.append(k)
            pass
        pass
    compare_list = [[kernel_flow[k]['cfg'].m_size, kernel_flow[k]['cfg'].n_size, kernel_flow[k]['cfg'].k_size] for k in keys]
    unique_list = []
    unique_id_list = []
    for i in range(len(compare_list)):
        obj = compare_list[i]
        if obj not in unique_list:
            unique_list.append(obj)
            unique_id_list.append(i)
    for i in unique_id_list:
        kernel_flow[keys[i]]["repeat"] = compare_list.count(compare_list[i])
        pass
    keys_to_delete = []
    for i in range(len(keys)):
        if i not in unique_id_list: keys_to_delete.append(keys[i])
        pass
    for k in keys_to_delete: del kernel_flow[k]

    return kernel_flow
    pass

def hbm_plan_summary(plan_in):
    plan = copy.deepcopy(plan_in)
    # Remove belonging
    keys_to_delete = []
    for k, v in plan.items():
        if "belongs" in v:
            keys_to_delete.append(k)
            pass
        pass
    for k in keys_to_delete: del plan[k]

    # Summaries MoE Weights
    keys_to_delete = []
    pattern = r"^moe_routed_\d+_up_proj_weight$"
    size = 0
    for k, v in plan.items():
        if re.match(pattern, k):
            size += v["size"]
            keys_to_delete.append(k)
            pass
        pass
    for k in keys_to_delete: del plan[k]
    if len(keys_to_delete) > 0: plan["moe_routed_up_proj_weight"] = {"size" : size}

    keys_to_delete = []
    pattern = r"^moe_routed_\d+_gate_proj_weight$"
    size = 0
    for k, v in plan.items():
        if re.match(pattern, k):
            size += v["size"]
            keys_to_delete.append(k)
            pass
        pass
    for k in keys_to_delete: del plan[k]
    if len(keys_to_delete) > 0: plan["moe_routed_gate_proj_weight"] = {"size" : size}

    keys_to_delete = []
    pattern = r"^moe_routed_\d+_down_proj_weight$"
    size = 0
    for k, v in plan.items():
        if re.match(pattern, k):
            size += v["size"]
            keys_to_delete.append(k)
            pass
        pass
    for k in keys_to_delete: del plan[k]
    if len(keys_to_delete) > 0: plan["moe_routed_down_proj_weight"] = {"size" : size}

    #Report
    cv.show_breakdown(plan, metric='size', unit='KiB', scale_div=1024)
    pass

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


def deepseek_layer_plan(llm, work, arch, EP=1, moe_distribution = 'Fair', attn_qn_proj_TP = 1, attn_o2_proj_TP = 1, use_flash_attn = False, gen_c2c_flow = False):

    #Basic Settings
    elem_size                           = 1 if llm.dtype == 'fp8' else 2
    index_size                          = 4 #uint32_t
    c2c_flow                            = {}
    kernel_flow                         = {}
    spaceA_hbm_plan                     = {}
    spaceB_hbm_plan                     = {}
    spaceA_hbm_addr                     = 0
    spaceB_hbm_addr                     = 0

    if work.prefill_enabled:
        sequence_length                 = work.prefill_input_token
        kv_cached_length                = 0
        q_sequence_length               = work.prefill_input_token
        speculative_length              = 1
    else:
        sequence_length                 = work.speculative_factor
        kv_cached_length                = work.kv_cache_length
        q_sequence_length               = 1
        speculative_length              = work.speculative_factor
        pass


    spaceB_hbm_plan["layer_input"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.embeded_length),
        "shape"                         :(work.batch_size * sequence_length,  llm.embeded_length),
        "size"                          : work.batch_size * sequence_length * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += work.batch_size * sequence_length * llm.embeded_length * elem_size


    spaceB_hbm_plan["cn_caches"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(work.batch_size,  llm.max_sequence_length,  llm.kv_lora_rank),
        "shape"                         :(work.batch_size * llm.max_sequence_length,  llm.kv_lora_rank),
        "size"                          : work.batch_size * llm.max_sequence_length * llm.kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += work.batch_size * llm.max_sequence_length * llm.kv_lora_rank * elem_size

    spaceB_hbm_plan["cr_caches"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(work.batch_size,  llm.max_sequence_length,  llm.rope_head_dim),
        "shape"                         :(work.batch_size * llm.max_sequence_length,  llm.rope_head_dim),
        "size"                          : work.batch_size * llm.max_sequence_length * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += work.batch_size * llm.max_sequence_length * llm.rope_head_dim * elem_size

    spaceA_hbm_plan["position"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length),
        "shape"                         :(work.batch_size * sequence_length,),
        "size"                          : work.batch_size * sequence_length * index_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * index_size
    spaceA_hbm_addr                       = align_addr(spaceA_hbm_addr)


    #################################
    #       1. Normalization        #
    #################################

    spaceA_hbm_plan["attn_norm"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.embeded_length),
        "shape"                         :(work.batch_size * sequence_length,  llm.embeded_length),
        "size"                          : work.batch_size * sequence_length * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.embeded_length * elem_size

    attn_norm_cfg                       = RMSNorm()
    attn_norm_cfg.dtype                 = llm.dtype
    attn_norm_cfg.m_size                = work.batch_size * sequence_length
    attn_norm_cfg.n_size                = llm.embeded_length
    attn_norm_cfg.norm_numer            = work.numerical_check_enable

    kernel_flow["attn_norm"] = {
        "type"                          : "norm",
        "input"                         : {"on": "spaceB",   "name": "layer_input"},
        "output"                        : {"on": "spaceA",    "name": "attn_norm"},
        "cfg"                           : attn_norm_cfg
    }


    #########################################################
    #       2. Latent Projection (C^Q, C^KV merged)         #
    #########################################################

    spaceB_hbm_plan["latent_qc_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.embeded_length,  (llm.q_lora_rank + llm.kv_lora_rank)),
        "size"                          : llm.embeded_length * (llm.q_lora_rank + llm.kv_lora_rank) * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.embeded_length * (llm.q_lora_rank + llm.kv_lora_rank) * elem_size

    spaceA_hbm_plan["attn_latqc"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  (llm.q_lora_rank + llm.kv_lora_rank)),
        "shape"                         :(work.batch_size * sequence_length,  (llm.q_lora_rank + llm.kv_lora_rank)),
        "size"                          : work.batch_size * sequence_length * (llm.q_lora_rank + llm.kv_lora_rank) * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * (llm.q_lora_rank + llm.kv_lora_rank) * elem_size

    attn_latqc_proj                     = SummaGEMM()
    attn_latqc_proj.dtype               = llm.dtype
    attn_latqc_proj.m_size              = work.batch_size * sequence_length
    attn_latqc_proj.n_size              = llm.q_lora_rank + llm.kv_lora_rank
    attn_latqc_proj.k_size              = llm.embeded_length
    attn_latqc_proj.resha_x_from_enable = 0
    attn_latqc_proj.resha_z_to_enable   = 0
    attn_latqc_proj.summa_numer         = work.numerical_check_enable

    kernel_flow["attn_latqc_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "attn_norm"},
        "weight"                        : {"on": "spaceB",   "name": "latent_qc_weight"},
        "output"                        : {"on": "spaceA",    "name": "attn_latqc"},
        "cfg"                           : attn_latqc_proj
    }

    #########################################
    #       3. Latent Projection CR         #
    #########################################

    spaceB_hbm_plan["latent_cr_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.rope_head_dim),
        "size"                          : llm.embeded_length * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.embeded_length * llm.rope_head_dim * elem_size


    spaceB_hbm_plan["attn_latcr"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.rope_head_dim),
        "shape"                         :(work.batch_size * sequence_length,  llm.rope_head_dim),
        "size"                          : work.batch_size * sequence_length * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += work.batch_size * sequence_length * llm.rope_head_dim * elem_size

    attn_latcr_proj                     = SummaGEMM()
    attn_latcr_proj.dtype               = llm.dtype
    attn_latcr_proj.m_size              = work.batch_size * sequence_length
    attn_latcr_proj.n_size              = llm.rope_head_dim
    attn_latcr_proj.k_size              = llm.embeded_length
    attn_latcr_proj.resha_x_from_enable = 0
    attn_latcr_proj.resha_z_to_enable   = 0
    attn_latcr_proj.summa_numer         = work.numerical_check_enable

    kernel_flow["attn_latcr_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "attn_norm"},
        "weight"                        : {"on": "spaceB",   "name": "latent_cr_weight"},
        "output"                        : {"on": "spaceB",   "name": "attn_latcr"},
        "cfg"                           : attn_latcr_proj
    }

    ############################
    #       4. CR RoPE         #
    ############################

    spaceA_hbm_plan["rope_c_cos_table"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.rope_head_dim // 2)),
        "size"                          : llm.max_sequence_length * (llm.rope_head_dim // 2) * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                      += llm.max_sequence_length * (llm.rope_head_dim // 2) * elem_size

    spaceA_hbm_plan["rope_c_sin_table"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.rope_head_dim // 2)),
        "size"                          : llm.max_sequence_length * (llm.rope_head_dim // 2) * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                      += llm.max_sequence_length * (llm.rope_head_dim // 2) * elem_size

    attn_rope_cr                        = RoPE()
    attn_rope_cr.dtype                  = llm.dtype
    attn_rope_cr.m_size                 = work.batch_size * sequence_length
    attn_rope_cr.n_size                 = llm.rope_head_dim
    attn_rope_cr.maximun_seqlen         = llm.max_sequence_length
    attn_rope_cr.contiguous_length      = sequence_length
    attn_rope_cr.view_enable            = 0
    attn_rope_cr.rope_numer             = work.numerical_check_enable

    kernel_flow["attn_rope_cr"] = {
        "type"                          : "rope",
        "input"                         : {"on": "spaceB",  "name": "attn_latcr"},
        "output"                        : {"on": "spaceB",  "name": "attn_latcr"},
        "cos"                           : {"on": "spaceA",   "name": "rope_c_cos_table"},
        "sin"                           : {"on": "spaceA",   "name": "rope_c_sin_table"},
        "position"                      : {"on": "spaceA",   "name": "position"},
        "cfg"                           : attn_rope_cr
    }


    ####################################
    #       5. Layout Remaping         #
    ####################################

    spaceA_hbm_plan["attn_latq"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.q_lora_rank),
        "shape"                         :(work.batch_size * sequence_length,  llm.q_lora_rank),
        "size"                          : work.batch_size * sequence_length * llm.q_lora_rank * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.q_lora_rank * elem_size

    kernel_flow["attn_split_concat"] = {
        "type"                          : "split_concat",
        "input1"                        : {"on": "spaceA",    "name": "attn_latqc"},
        "input2"                        : {"on": "spaceB",   "name": "attn_latcr"},
        "output1"                       : {"on": "spaceA",    "name": "attn_latq"},
        "output2"                       : {"on": "spaceB",   "name": "cn_caches", "sequence_offset": kv_cached_length},
        "output3"                       : {"on": "spaceB",   "name": "cr_caches", "sequence_offset": kv_cached_length},
        "cfg"                           : None
    }


    ##################################
    #       6. QN Projection         #
    ##################################

    spaceB_hbm_plan["qn_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.q_lora_rank,  llm.num_heads * llm.kv_lora_rank),
        "size"                          : llm.q_lora_rank * llm.num_heads * llm.kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.q_lora_rank * llm.num_heads * llm.kv_lora_rank * elem_size

    spaceA_hbm_plan["attn_qn"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.num_heads,  llm.kv_lora_rank),
        "shape"                         :(work.batch_size * sequence_length,  llm.num_heads * llm.kv_lora_rank),
        "size"                          : work.batch_size * sequence_length * llm.num_heads * llm.kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.num_heads * llm.kv_lora_rank * elem_size

    if attn_qn_proj_TP == 1:
        attn_qn_proj                        = SummaGEMM()
        attn_qn_proj.dtype                  = llm.dtype
        attn_qn_proj.m_size                 = work.batch_size * sequence_length
        attn_qn_proj.n_size                 = llm.num_heads * llm.kv_lora_rank
        attn_qn_proj.k_size                 = llm.q_lora_rank
        attn_qn_proj.resha_x_from_enable    = 0
        attn_qn_proj.resha_z_to_enable      = 0
        attn_qn_proj.summa_numer            = work.numerical_check_enable
    else:
        attn_qn_proj                        = SummaGEMM()
        attn_qn_proj.dtype                  = llm.dtype
        attn_qn_proj.m_size                 = work.batch_size * sequence_length * attn_qn_proj_TP
        attn_qn_proj.n_size                 = (llm.num_heads * llm.kv_lora_rank + attn_qn_proj_TP - 1) // attn_qn_proj_TP
        attn_qn_proj.k_size                 = llm.q_lora_rank
        attn_qn_proj.resha_x_from_enable    = 0
        attn_qn_proj.resha_z_to_enable      = 0
        attn_qn_proj.summa_numer            = work.numerical_check_enable

        #Add C2C Flow
        c2c_flow['attn_qn_proj_TP_All_Gather'] = {
            "type"                          : "All_Gather",
            "parallelism"                   : attn_qn_proj_TP,
            "m_size"                        : work.batch_size * sequence_length,
            "n_size"                        : llm.q_lora_rank,
            "to"                            : "m",
            "elem_size"                     : elem_size
        }
        c2c_flow['attn_qn_proj_TP_All_to_All'] = {
            "type"                          : "All_to_All",
            "parallelism"                   : attn_qn_proj_TP,
            "m_size"                        : work.batch_size * sequence_length * attn_qn_proj_TP,
            "n_size"                        : (llm.num_heads * llm.kv_lora_rank + attn_qn_proj_TP - 1) // attn_qn_proj_TP,
            "from"                          : "m",
            "to"                            : "n",
            "elem_size"                     : elem_size
        }
        pass

    kernel_flow["attn_qn_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "attn_latq"},
        "weight"                        : {"on": "spaceB",   "name": "qn_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "attn_qn"},
        "cfg"                           : attn_qn_proj
    }

    ##################################
    #       7. QR Projection         #
    ##################################

    spaceB_hbm_plan["qr_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.q_lora_rank,  llm.num_heads * llm.rope_head_dim),
        "size"                          : llm.q_lora_rank * llm.num_heads * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.q_lora_rank * llm.num_heads * llm.rope_head_dim * elem_size

    spaceA_hbm_plan["attn_qr"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.num_heads,  llm.rope_head_dim),
        "shape"                         :(work.batch_size * sequence_length,  llm.num_heads * llm.rope_head_dim),
        "size"                          : work.batch_size * sequence_length * llm.num_heads * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.num_heads * llm.rope_head_dim * elem_size

    attn_qr_proj                        = SummaGEMM()
    attn_qr_proj.dtype                  = llm.dtype
    attn_qr_proj.m_size                 = work.batch_size * sequence_length
    attn_qr_proj.n_size                 = llm.num_heads * llm.rope_head_dim
    attn_qr_proj.k_size                 = llm.q_lora_rank
    attn_qr_proj.resha_x_from_enable    = 0
    attn_qr_proj.resha_z_to_enable      = 0
    attn_qr_proj.summa_numer            = work.numerical_check_enable

    kernel_flow["attn_qr_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "attn_latq"},
        "weight"                        : {"on": "spaceB",   "name": "qr_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "attn_qr"},
        "cfg"                           : attn_qr_proj
    }

    ############################
    #       8. QR RoPE         #
    ############################

    spaceB_hbm_plan["rope_q_cos_table"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.num_heads * llm.rope_head_dim // 2)),
        "size"                          : llm.max_sequence_length * (llm.num_heads * llm.rope_head_dim // 2) * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.max_sequence_length * (llm.num_heads * llm.rope_head_dim // 2) * elem_size

    spaceB_hbm_plan["rope_q_sin_table"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.num_heads * llm.rope_head_dim // 2)),
        "size"                          : llm.max_sequence_length * (llm.num_heads * llm.rope_head_dim // 2) * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.max_sequence_length * (llm.num_heads * llm.rope_head_dim // 2) * elem_size

    attn_rope_qr                        = RoPE()
    attn_rope_qr.dtype                  = llm.dtype
    attn_rope_qr.m_size                 = work.batch_size * sequence_length
    attn_rope_qr.n_size                 = llm.num_heads * llm.rope_head_dim
    attn_rope_qr.maximun_seqlen         = llm.max_sequence_length
    attn_rope_qr.contiguous_length      = sequence_length
    attn_rope_qr.view_enable            = 0
    attn_rope_qr.rope_numer             = work.numerical_check_enable

    kernel_flow["attn_rope_qr"] = {
        "type"                          : "rope",
        "input"                         : {"on": "spaceA",   "name": "attn_qr"},
        "output"                        : {"on": "spaceA",   "name": "attn_qr"},
        "cos"                           : {"on": "spaceB",  "name": "rope_q_cos_table"},
        "sin"                           : {"on": "spaceB",  "name": "rope_q_sin_table"},
        "position"                      : {"on": "spaceA",   "name": "position"},
        "cfg"                           : attn_rope_qr
    }

    ############################
    #       9. FlatMLA         #
    ############################

    spaceA_hbm_plan["attn_o"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.num_heads,  llm.kv_lora_rank),
        "shape"                         :(work.batch_size * sequence_length,  llm.num_heads * llm.kv_lora_rank),
        "size"                          : work.batch_size * sequence_length * llm.num_heads * llm.kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.num_heads * llm.kv_lora_rank * elem_size

    attn_flatmla                        = FlatMLA()
    attn_flatmla.dtype                  = llm.dtype
    attn_flatmla.kv_sequence_length     = kv_cached_length + sequence_length
    attn_flatmla.q_sequence_length      = q_sequence_length
    attn_flatmla.speculative_length     = speculative_length
    attn_flatmla.nope_head_dim          = llm.kv_lora_rank
    attn_flatmla.rope_head_dim          = llm.rope_head_dim
    attn_flatmla.num_head               = llm.num_heads
    attn_flatmla.batch_size             = work.batch_size
    attn_flatmla.flatten_numer          = work.numerical_check_enable
    attn_flatmla.use_flash_attn         = use_flash_attn

    kernel_flow["attn_flatmla"] = {
        "type"                          : "flatmla",
        "qn"                            : {"on": "spaceA",   "name": "attn_qn"},
        "qr"                            : {"on": "spaceA",   "name": "attn_qr"},
        "cn"                            : {"on": "spaceB",  "name": "cn_caches"},
        "cr"                            : {"on": "spaceB",  "name": "cr_caches"},
        "o"                             : {"on": "spaceA",   "name": "attn_o"},
        "cfg"                           : attn_flatmla
    }

    #############################################
    #       10. O First Down Projection         #
    #############################################

    spaceB_hbm_plan["o1_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.kv_lora_rank,  llm.num_heads * llm.head_dimension),
        "size"                          : llm.kv_lora_rank * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.kv_lora_rank * llm.num_heads * llm.head_dimension * elem_size

    spaceA_hbm_plan["attn_o1"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.num_heads,  llm.head_dimension),
        "shape"                         :(work.batch_size * sequence_length,  llm.num_heads * llm.head_dimension),
        "size"                          : work.batch_size * sequence_length * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.num_heads * llm.head_dimension * elem_size

    attn_o1_proj                        = SummaGEMM()
    attn_o1_proj.dtype                  = llm.dtype
    attn_o1_proj.m_size                 = work.batch_size * sequence_length
    attn_o1_proj.n_size                 = llm.head_dimension
    attn_o1_proj.k_size                 = llm.num_heads * llm.kv_lora_rank
    attn_o1_proj.ofdp_splitk_num        = llm.num_heads
    attn_o1_proj.resha_x_from_enable    = 0
    attn_o1_proj.resha_z_to_enable      = 0
    attn_o1_proj.summa_numer            = work.numerical_check_enable

    kernel_flow["attn_o1_proj"] = {
        "type"                          : "ofdp",
        "input"                         : {"on": "spaceA",    "name": "attn_o"},
        "weight"                        : {"on": "spaceB",   "name": "o1_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "attn_o1"},
        "cfg"                           : attn_o1_proj
    }

    #############################################
    #       11. O Final Down Projection         #
    #############################################

    spaceB_hbm_plan["o2_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.num_heads * llm.head_dimension,  llm.embeded_length),
        "size"                          : llm.num_heads * llm.head_dimension * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.num_heads * llm.head_dimension * llm.embeded_length * elem_size

    spaceA_hbm_plan["attn_o2"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.embeded_length),
        "shape"                         :(work.batch_size * sequence_length,  llm.embeded_length),
        "size"                          : work.batch_size * sequence_length * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.embeded_length * elem_size

    if attn_o2_proj_TP == 1:
        attn_o2_proj                    = SummaGEMM()
        attn_o2_proj.dtype              = llm.dtype
        attn_o2_proj.m_size             = work.batch_size * sequence_length
        attn_o2_proj.n_size             = llm.embeded_length
        attn_o2_proj.k_size             = llm.num_heads * llm.head_dimension
        attn_o2_proj.resha_x_from_enable= 0
        attn_o2_proj.resha_z_to_enable  = 0
        attn_o2_proj.summa_numer        = work.numerical_check_enable
    else:
        attn_o2_proj                    = SummaGEMM()
        attn_o2_proj.dtype              = llm.dtype
        attn_o2_proj.m_size             = work.batch_size * sequence_length * attn_o2_proj_TP
        attn_o2_proj.n_size             = llm.embeded_length
        attn_o2_proj.k_size             = (llm.num_heads * llm.head_dimension + attn_o2_proj_TP - 1) // attn_o2_proj_TP
        attn_o2_proj.resha_x_from_enable= 0
        attn_o2_proj.resha_z_to_enable  = 0
        attn_o2_proj.summa_numer        = work.numerical_check_enable

        # Add C2C Flow
        c2c_flow['attn_o2_proj_TP_All_to_All'] = {
            "type"                          : "All_to_All",
            "parallelism"                   : attn_o2_proj_TP,
            "m_size"                        : work.batch_size * sequence_length,
            "n_size"                        : llm.num_heads * llm.head_dimension,
            "from"                          : "n",
            "to"                            : "m",
            "elem_size"                     : elem_size
        }
        c2c_flow['attn_o2_proj_TP_Reduce_Scatter'] = {
            "type"                          : "Reduce_Scatter",
            "parallelism"                   : attn_o2_proj_TP,
            "m_size"                        : work.batch_size * sequence_length * attn_o2_proj_TP,
            "n_size"                        : llm.embeded_length,
            "on"                            : "m",
            "elem_size"                     : elem_size
        }
        pass

    kernel_flow["attn_o2_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "attn_o1"},
        "weight"                        : {"on": "spaceB",   "name": "o2_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "attn_o2"},
        "cfg"                           : attn_o2_proj
    }

    ##################################
    #       12. ResNet Addition      #
    ##################################
    attn_resnet                         = Activation()
    attn_resnet.dtype                   = llm.dtype
    attn_resnet.algo                    = 'none'
    attn_resnet.m_size                  = work.batch_size * sequence_length
    attn_resnet.n_size                  = llm.embeded_length
    attn_resnet.gate_enable             = 0
    attn_resnet.bias_enable             = 1
    attn_resnet.acti_numer              = work.numerical_check_enable

    kernel_flow["attn_resnet"] = {
        "type"                          : "addi",
        "input"                         : {"on": "spaceB",   "name": "layer_input"},
        "bias"                          : {"on": "spaceA",    "name": "attn_o2"},
        "output"                        : {"on": "spaceB",   "name": "layer_input"},
        "cfg"                           : attn_resnet
    }

    ##################################
    #       13. Normalization        #
    ##################################

    spaceA_hbm_plan["moe_norm"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length, llm.embeded_length),
        "shape"                         :(work.batch_size * sequence_length, llm.embeded_length),
        "size"                          : work.batch_size * sequence_length * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.embeded_length * elem_size

    moe_norm_cfg                        = RMSNorm()
    moe_norm_cfg.dtype                  = llm.dtype
    moe_norm_cfg.m_size                 = work.batch_size * sequence_length
    moe_norm_cfg.n_size                 = llm.embeded_length
    moe_norm_cfg.norm_numer             = work.numerical_check_enable

    kernel_flow["moe_norm"] = {
        "type"                          : "norm",
        "input"                         : {"on": "spaceB",   "name": "layer_input"},
        "output"                        : {"on": "spaceA",    "name": "moe_norm"},
        "cfg"                           : moe_norm_cfg
    }

    ###################################
    #       14. Shared Experts        #
    ###################################

    for eid in range(llm.n_shared_experts):
        prefix = f"moe_shared_{eid}_"

        #Up Projection
        spaceB_hbm_plan[f"{prefix}up_proj_weight"] = {
            "addr"                          : spaceB_hbm_addr,
            "shape"                         :(llm.embeded_length,  llm.moe_inter_dim),
            "size"                          : llm.embeded_length * llm.moe_inter_dim * elem_size,
            "tensor"                        : None
        }
        spaceB_hbm_addr                      += llm.embeded_length * llm.moe_inter_dim * elem_size

        spaceA_hbm_plan[f"{prefix}up"] = {
            "addr"                          : spaceA_hbm_addr,
            "view"                          :(work.batch_size,  sequence_length,  llm.moe_inter_dim),
            "shape"                         :(work.batch_size * sequence_length,  llm.moe_inter_dim),
            "size"                          : work.batch_size * sequence_length * llm.moe_inter_dim * elem_size,
            "tensor"                        : None
        }
        spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.moe_inter_dim * elem_size

        ffn_up_proj                         = SummaGEMM()
        ffn_up_proj.dtype                   = llm.dtype
        ffn_up_proj.m_size                  = work.batch_size * sequence_length
        ffn_up_proj.n_size                  = llm.moe_inter_dim
        ffn_up_proj.k_size                  = llm.embeded_length
        ffn_up_proj.resha_x_from_enable     = 0
        ffn_up_proj.resha_z_to_enable       = 0
        ffn_up_proj.summa_numer             = work.numerical_check_enable

        kernel_flow[f"{prefix}up"] = {
            "type"                          : "gemm",
            "input"                         : {"on": "spaceA",    "name": "moe_norm"},
            "weight"                        : {"on": "spaceB",   "name": f"{prefix}up_proj_weight"},
            "output"                        : {"on": "spaceA",    "name": f"{prefix}up"},
            "cfg"                           : ffn_up_proj
        }

        #Gate Projection
        spaceB_hbm_plan[f"{prefix}gate_proj_weight"] = {
            "addr"                          : spaceB_hbm_addr,
            "shape"                         :(llm.embeded_length,  llm.moe_inter_dim),
            "size"                          : llm.embeded_length * llm.moe_inter_dim * elem_size,
            "tensor"                        : None
        }
        spaceB_hbm_addr                      += llm.embeded_length * llm.moe_inter_dim * elem_size

        spaceA_hbm_plan[f"{prefix}gate"] = {
            "addr"                          : spaceA_hbm_addr,
            "view"                          :(work.batch_size,  sequence_length,  llm.moe_inter_dim),
            "shape"                         :(work.batch_size * sequence_length,  llm.moe_inter_dim),
            "size"                          : work.batch_size * sequence_length * llm.moe_inter_dim * elem_size,
            "tensor"                        : None
        }
        spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.moe_inter_dim * elem_size

        ffn_gate_proj                       = SummaGEMM()
        ffn_gate_proj.dtype                 = llm.dtype
        ffn_gate_proj.m_size                = work.batch_size * sequence_length
        ffn_gate_proj.n_size                = llm.moe_inter_dim
        ffn_gate_proj.k_size                = llm.embeded_length
        ffn_gate_proj.resha_x_from_enable   = 0
        ffn_gate_proj.resha_z_to_enable     = 0
        ffn_gate_proj.summa_numer           = work.numerical_check_enable

        kernel_flow[f"{prefix}gate"] = {
            "type"                          : "gemm",
            "input"                         : {"on": "spaceA",    "name": "moe_norm"},
            "weight"                        : {"on": "spaceB",   "name": f"{prefix}gate_proj_weight"},
            "output"                        : {"on": "spaceA",    "name": f"{prefix}gate"},
            "cfg"                           : ffn_gate_proj
        }

        #Activation
        spaceA_hbm_plan[f"{prefix}acti"] = {
            "addr"                          : spaceA_hbm_addr,
            "view"                          :(work.batch_size,  sequence_length,  llm.moe_inter_dim),
            "shape"                         :(work.batch_size * sequence_length,  llm.moe_inter_dim),
            "size"                          : work.batch_size * sequence_length * llm.moe_inter_dim * elem_size,
            "tensor"                        : None
        }
        spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.moe_inter_dim * elem_size

        ffn_acti                            = Activation()
        ffn_acti.dtype                      = llm.dtype
        ffn_acti.algo                       = llm.moe_acti_algo
        ffn_acti.m_size                     = work.batch_size * sequence_length
        ffn_acti.n_size                     = llm.moe_inter_dim
        ffn_acti.gate_enable                = 1
        ffn_acti.bias_enable                = 0
        ffn_acti.acti_numer                 = work.numerical_check_enable

        kernel_flow[f"{prefix}acti"] = {
            "type"                          : "acti",
            "input"                         : {"on": "spaceA",    "name": f"{prefix}up"},
            "gate"                          : {"on": "spaceA",    "name": f"{prefix}gate"},
            "output"                        : {"on": "spaceA",    "name": f"{prefix}acti"},
            "cfg"                           : ffn_acti
        }

        #Down Projection
        spaceB_hbm_plan[f"{prefix}down_proj_weight"] = {
            "addr"                          : spaceB_hbm_addr,
            "shape"                         :(llm.moe_inter_dim,  llm.embeded_length),
            "size"                          : llm.moe_inter_dim * llm.embeded_length * elem_size,
            "tensor"                        : None
        }
        spaceB_hbm_addr                      += llm.moe_inter_dim * llm.embeded_length * elem_size

        spaceA_hbm_plan[f"{prefix}down"] = {
            "addr"                          : spaceA_hbm_addr,
            "view"                          :(work.batch_size,  sequence_length,  llm.embeded_length),
            "shape"                         :(work.batch_size * sequence_length,  llm.embeded_length),
            "size"                          : work.batch_size * sequence_length * llm.embeded_length * elem_size,
            "tensor"                        : None
        }
        spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.embeded_length * elem_size

        ffn_down_proj                       = SummaGEMM()
        ffn_down_proj.dtype                 = llm.dtype
        ffn_down_proj.m_size                = work.batch_size * sequence_length
        ffn_down_proj.n_size                = llm.embeded_length
        ffn_down_proj.k_size                = llm.moe_inter_dim
        ffn_down_proj.resha_x_from_enable   = 0
        ffn_down_proj.resha_z_to_enable     = 0
        ffn_down_proj.summa_numer           = work.numerical_check_enable

        kernel_flow[f"{prefix}down"] = {
            "type"                          : "gemm",
            "input"                         : {"on": "spaceA",    "name": f"{prefix}acti"},
            "weight"                        : {"on": "spaceB",   "name": f"{prefix}down_proj_weight"},
            "output"                        : {"on": "spaceA",    "name": f"{prefix}down"},
            "cfg"                           : ffn_gate_proj
        }
        pass

    #####################################
    #       15. MoE Route Gating        #
    #####################################

    spaceB_hbm_plan["moe_rgate_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.n_routed_experts),
        "size"                          : llm.embeded_length * llm.n_routed_experts * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.embeded_length * llm.n_routed_experts * elem_size

    spaceA_hbm_plan["moe_rgate"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.n_routed_experts),
        "shape"                         :(work.batch_size * sequence_length,  llm.n_routed_experts),
        "size"                          : work.batch_size * sequence_length * llm.n_routed_experts * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.n_routed_experts * elem_size

    moe_rgate_proj                      = SummaGEMM()
    moe_rgate_proj.dtype                = llm.dtype
    moe_rgate_proj.m_size               = work.batch_size * sequence_length
    moe_rgate_proj.n_size               = llm.n_routed_experts
    moe_rgate_proj.k_size               = llm.embeded_length
    moe_rgate_proj.resha_x_from_enable  = 0
    moe_rgate_proj.resha_z_to_enable    = 0
    moe_rgate_proj.summa_numer          = work.numerical_check_enable

    kernel_flow["moe_rgate_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "moe_norm"},
        "weight"                        : {"on": "spaceB",   "name": "moe_rgate_weight"},
        "output"                        : {"on": "spaceA",    "name": "moe_rgate"},
        "cfg"                           : moe_rgate_proj
    }

    ###################################
    #       16. MoE Route TopK        #
    ###################################

    D_all, P_all, SCORE = gen_moe_gate_index(work.batch_size * sequence_length * EP, llm.n_routed_experts, llm.n_activated_experts, moe_distribution = moe_distribution)

    #Find the most heavy workload
    EP_tokens           = [sum(SCORE[e::EP]) for e in range(EP)]
    max_eid             = EP_tokens.index(max(EP_tokens))
    D                   = D_all[max_eid::EP]
    P                   = P_all[max_eid::EP]
    SCORE               = SCORE[max_eid::EP]
    ep_experts          = llm.n_routed_experts // EP

    spaceB_hbm_plan["moe_route_val"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.n_activated_experts),
        "shape"                         :(work.batch_size * sequence_length,  llm.n_activated_experts),
        "size"                          : work.batch_size * sequence_length * llm.n_activated_experts * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += work.batch_size * sequence_length * llm.n_activated_experts * elem_size
    spaceB_hbm_addr                      = align_addr(spaceB_hbm_addr)

    spaceB_hbm_plan["moe_route_idx"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length,  llm.n_activated_experts),
        "shape"                         :(work.batch_size * sequence_length,  llm.n_activated_experts),
        "size"                          : work.batch_size * sequence_length * llm.n_activated_experts * index_size,
        "is_index"                      : True,
        "tensor"                        : D
    }
    spaceB_hbm_addr                      += work.batch_size * sequence_length * llm.n_activated_experts * index_size
    spaceB_hbm_addr                      = align_addr(spaceB_hbm_addr)

    moe_rgate_topk                      = MoEGate()
    moe_rgate_topk.dtype                = llm.dtype
    moe_rgate_topk.num_tokens           = work.batch_size * sequence_length
    moe_rgate_topk.num_routed_experts   = llm.n_routed_experts
    moe_rgate_topk.num_active_experts   = llm.n_activated_experts
    moe_rgate_topk.moeg_numer           = work.numerical_check_enable

    kernel_flow["moe_rgate_topk"] = {
        "type"                          : "moeg",
        "input"                         : {"on": "spaceA",    "name": "moe_rgate"},
        "output_val"                    : {"on": "spaceB",   "name": "moe_route_val"},
        "output_idx"                    : {"on": "spaceB",   "name": "moe_route_idx"},
        "cfg"                           : moe_rgate_topk
    }

    #################################
    #       17. MoE Dispatch        #
    #################################

    spaceA_hbm_plan["moe_dispatch_buffer"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(ep_experts,  work.batch_size * sequence_length,  llm.embeded_length),
        "shape"                         :(ep_experts * work.batch_size * sequence_length,  llm.embeded_length),
        "size"                          : ep_experts * work.batch_size * sequence_length * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += ep_experts * work.batch_size * sequence_length * llm.embeded_length * elem_size

    #Generate Memory Notations
    for eid in range(ep_experts):
        prefix = f"moe_routed_{eid}_"
        num_tokens = SCORE[eid].item()
        if num_tokens == 0:
            continue
            pass

        spaceA_hbm_plan[f"{prefix}input"] = {
            "addr"                          : spaceA_hbm_plan["moe_dispatch_buffer"]["addr"] + eid * work.batch_size * sequence_length * llm.embeded_length * elem_size,
            "view"                          :(num_tokens,  llm.embeded_length),
            "shape"                         :(num_tokens,  llm.embeded_length),
            "size"                          : num_tokens * llm.embeded_length * elem_size,
            "belongs"                       : "moe_dispatch_buffer",
            "tensor"                        : None
        }
        pass

    if EP == 1:
        spaceB_hbm_plan["moe_route_pos"] = {
            "addr"                          : spaceB_hbm_addr,
            "view"                          :(work.batch_size,  sequence_length,  llm.n_activated_experts),
            "shape"                         :(work.batch_size * sequence_length,  llm.n_activated_experts),
            "size"                          : work.batch_size * sequence_length * llm.n_activated_experts * index_size,
            "is_index"                      : True,
            "tensor"                        : P
        }
        spaceB_hbm_addr                      += work.batch_size * sequence_length * llm.n_activated_experts * index_size
        spaceB_hbm_addr                      = align_addr(spaceB_hbm_addr)

        moe_dispatch                        = MoEDispatch()
        moe_dispatch.dtype                  = llm.dtype
        moe_dispatch.num_tokens             = work.batch_size * sequence_length
        moe_dispatch.embedded_length        = llm.embeded_length
        moe_dispatch.num_routed_experts     = llm.n_routed_experts
        moe_dispatch.num_active_experts     = llm.n_activated_experts
        moe_dispatch.nodis_enable           = 1
        moe_dispatch.moed_numer             = work.numerical_check_enable

        kernel_flow["moe_dispatch"] = {
            "type"                          : "moed",
            "input"                         : {"on": "spaceA",    "name": "moe_norm"},
            "input_idx"                     : {"on": "spaceB",   "name": "moe_route_idx"},
            "info"                          : {"merged_output" : {"on": "spaceA",    "name": "moe_dispatch_buffer"}},
            "output_pos"                    : {"on": "spaceB",   "name": "moe_route_pos"},
            "cfg"                           : moe_dispatch
        }

        for eid in range(llm.n_routed_experts):
            prefix = f"moe_routed_{eid}_"
            num_tokens = SCORE[eid].item()
            if num_tokens == 0:
                continue
                pass
            kernel_flow["moe_dispatch"][f"output_{eid}"] = {"on": "spaceA",    "name": f"{prefix}input"}
            pass
    else:
        c2c_flow['moe_dispatch'] = {
            "type"                          : "Disp",
            "parallelism"                   : EP,
            "embeded_length"                : llm.embeded_length,
            "num_tokens"                    : work.batch_size * sequence_length,
            "num_routed_experts"            : llm.n_routed_experts,
            "index"                         : D_all,
            "elem_size"                     : elem_size
        }
        pass

    ###################################
    #       18. Routed Experts        #
    ###################################

    #Shared Memory Spaces
    spaceA_hbm_plan["moe_routed_up"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size * sequence_length,  llm.moe_inter_dim),
        "shape"                         :(work.batch_size * sequence_length,  llm.moe_inter_dim),
        "size"                          : work.batch_size * sequence_length * llm.moe_inter_dim * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.moe_inter_dim * elem_size

    spaceA_hbm_plan["moe_routed_gate"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size * sequence_length,  llm.moe_inter_dim),
        "shape"                         :(work.batch_size * sequence_length,  llm.moe_inter_dim),
        "size"                          : work.batch_size * sequence_length * llm.moe_inter_dim * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.moe_inter_dim * elem_size

    spaceA_hbm_plan["moe_routed_acti"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size * sequence_length,  llm.moe_inter_dim),
        "shape"                         :(work.batch_size * sequence_length,  llm.moe_inter_dim),
        "size"                          : work.batch_size * sequence_length * llm.moe_inter_dim * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.moe_inter_dim * elem_size

    for eid in range(ep_experts):
        prefix = f"moe_routed_{eid}_"
        num_tokens = SCORE[eid].item()
        if num_tokens == 0:
            continue
            pass

        #Up Projection
        spaceB_hbm_plan[f"{prefix}up_proj_weight"] = {
            "addr"                          : spaceB_hbm_addr,
            "shape"                         :(llm.embeded_length,  llm.moe_inter_dim),
            "size"                          : llm.embeded_length * llm.moe_inter_dim * elem_size,
            "tensor"                        : None
        }
        spaceB_hbm_addr                      += llm.embeded_length * llm.moe_inter_dim * elem_size

        spaceA_hbm_plan[f"{prefix}up"] = {
            "addr"                          : spaceA_hbm_plan["moe_routed_up"]["addr"],
            "view"                          :(num_tokens,  llm.moe_inter_dim),
            "shape"                         :(num_tokens,  llm.moe_inter_dim),
            "size"                          : num_tokens * llm.moe_inter_dim * elem_size,
            "belongs"                       : "moe_routed_up",
            "tensor"                        : None
        }

        ffn_up_proj                         = SummaGEMM()
        ffn_up_proj.dtype                   = llm.dtype
        ffn_up_proj.m_size                  = num_tokens
        ffn_up_proj.n_size                  = llm.moe_inter_dim
        ffn_up_proj.k_size                  = llm.embeded_length
        ffn_up_proj.resha_x_from_enable     = 0
        ffn_up_proj.resha_z_to_enable       = 0
        ffn_up_proj.summa_numer             = work.numerical_check_enable

        kernel_flow[f"{prefix}up"] = {
            "type"                          : "gemm",
            "input"                         : {"on": "spaceA",    "name": f"{prefix}input"},
            "weight"                        : {"on": "spaceB",   "name": f"{prefix}up_proj_weight"},
            "output"                        : {"on": "spaceA",    "name": f"{prefix}up"},
            "cfg"                           : ffn_up_proj
        }

        #Gate Projection
        spaceB_hbm_plan[f"{prefix}gate_proj_weight"] = {
            "addr"                          : spaceB_hbm_addr,
            "shape"                         :(llm.embeded_length,  llm.moe_inter_dim),
            "size"                          : llm.embeded_length * llm.moe_inter_dim * elem_size,
            "tensor"                        : None
        }
        spaceB_hbm_addr                      += llm.embeded_length * llm.moe_inter_dim * elem_size

        spaceA_hbm_plan[f"{prefix}gate"] = {
            "addr"                          : spaceA_hbm_plan["moe_routed_gate"]["addr"],
            "view"                          :(num_tokens,  llm.moe_inter_dim),
            "shape"                         :(num_tokens,  llm.moe_inter_dim),
            "size"                          : num_tokens * llm.moe_inter_dim * elem_size,
            "belongs"                       : "moe_routed_gate",
            "tensor"                        : None
        }

        ffn_gate_proj                       = SummaGEMM()
        ffn_gate_proj.dtype                 = llm.dtype
        ffn_gate_proj.m_size                = num_tokens
        ffn_gate_proj.n_size                = llm.moe_inter_dim
        ffn_gate_proj.k_size                = llm.embeded_length
        ffn_gate_proj.resha_x_from_enable   = 0
        ffn_gate_proj.resha_z_to_enable     = 0
        ffn_gate_proj.summa_numer           = work.numerical_check_enable

        kernel_flow[f"{prefix}gate"] = {
            "type"                          : "gemm",
            "input"                         : {"on": "spaceA",    "name": f"{prefix}input"},
            "weight"                        : {"on": "spaceB",   "name": f"{prefix}gate_proj_weight"},
            "output"                        : {"on": "spaceA",    "name": f"{prefix}gate"},
            "cfg"                           : ffn_gate_proj
        }

        #Activation
        spaceA_hbm_plan[f"{prefix}acti"] = {
            "addr"                          : spaceA_hbm_plan["moe_routed_acti"]["addr"],
            "view"                          :(num_tokens,  llm.moe_inter_dim),
            "shape"                         :(num_tokens,  llm.moe_inter_dim),
            "size"                          : num_tokens * llm.moe_inter_dim * elem_size,
            "belongs"                       : "moe_routed_acti",
            "tensor"                        : None
        }

        ffn_acti                            = Activation()
        ffn_acti.dtype                      = llm.dtype
        ffn_acti.algo                       = llm.moe_acti_algo
        ffn_acti.m_size                     = num_tokens
        ffn_acti.n_size                     = llm.moe_inter_dim
        ffn_acti.gate_enable                = 1
        ffn_acti.bias_enable                = 0
        ffn_acti.acti_numer                 = work.numerical_check_enable

        kernel_flow[f"{prefix}acti"] = {
            "type"                          : "acti",
            "input"                         : {"on": "spaceA",    "name": f"{prefix}up"},
            "gate"                          : {"on": "spaceA",    "name": f"{prefix}gate"},
            "output"                        : {"on": "spaceA",    "name": f"{prefix}acti"},
            "cfg"                           : ffn_acti
        }

        #Down Projection
        spaceB_hbm_plan[f"{prefix}down_proj_weight"] = {
            "addr"                          : spaceB_hbm_addr,
            "shape"                         :(llm.moe_inter_dim,  llm.embeded_length),
            "size"                          : llm.moe_inter_dim * llm.embeded_length * elem_size,
            "tensor"                        : None
        }
        spaceB_hbm_addr                      += llm.moe_inter_dim * llm.embeded_length * elem_size

        ffn_down_proj                       = SummaGEMM()
        ffn_down_proj.dtype                 = llm.dtype
        ffn_down_proj.m_size                = num_tokens
        ffn_down_proj.n_size                = llm.embeded_length
        ffn_down_proj.k_size                = llm.moe_inter_dim
        ffn_down_proj.resha_x_from_enable   = 0
        ffn_down_proj.resha_z_to_enable     = 0
        ffn_down_proj.summa_numer           = work.numerical_check_enable

        kernel_flow[f"{prefix}down"] = {
            "type"                          : "gemm",
            "input"                         : {"on": "spaceA",    "name": f"{prefix}acti"},
            "weight"                        : {"on": "spaceB",   "name": f"{prefix}down_proj_weight"},
            "output"                        : {"on": "spaceA",    "name": f"{prefix}input"},
            "cfg"                           : ffn_gate_proj
        }
        pass

    ################################
    #       19. MoE Combine        #
    ################################

    spaceA_hbm_plan["moe_route_output"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length, llm.embeded_length),
        "shape"                         :(work.batch_size * sequence_length, llm.embeded_length),
        "size"                          : work.batch_size * sequence_length * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.embeded_length * elem_size

    if EP == 1:
        moe_combine                         = MoECombine()
        moe_combine.dtype                   = llm.dtype
        moe_combine.num_tokens              = work.batch_size * sequence_length
        moe_combine.embedded_length         = llm.embeded_length
        moe_combine.num_routed_experts      = llm.n_routed_experts
        moe_combine.num_active_experts      = llm.n_activated_experts
        moe_combine.moec_numer              = work.numerical_check_enable

        kernel_flow["moe_combine"] = {
            "type"                          : "moec",
            "input_val"                     : {"on": "spaceB",   "name": "moe_route_val"},
            "input_idx"                     : {"on": "spaceB",   "name": "moe_route_idx"},
            "input_pos"                     : {"on": "spaceB",   "name": "moe_route_pos"},
            "info"                          : {"merged_input" : {"on": "spaceA",    "name": "moe_dispatch_buffer"}},
            "output"                        : {"on": "spaceA",    "name": "moe_route_output"},
            "cfg"                           : moe_combine
        }

        for eid in range(llm.n_routed_experts):
            prefix = f"moe_routed_{eid}_"
            num_tokens = SCORE[eid].item()
            if num_tokens == 0:
                continue
                pass
            kernel_flow["moe_combine"][f"input_{eid}"] = {"on": "spaceA",    "name": f"{prefix}input"}
            pass
    else:
        c2c_flow['moe_combine'] = {
            "type"                          : "Comb",
            "parallelism"                   : EP,
            "embeded_length"                : llm.embeded_length,
            "num_tokens"                    : work.batch_size * sequence_length,
            "num_routed_experts"            : llm.n_routed_experts,
            "index"                         : D_all,
            "elem_size"                     : elem_size
        }
        pass

    ############################################################
    #       20. Merge Shared and Routed Experts Results        #
    ############################################################

    spaceA_hbm_plan["moe_o"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size,  sequence_length, llm.embeded_length),
        "shape"                         :(work.batch_size * sequence_length, llm.embeded_length),
        "size"                          : work.batch_size * sequence_length * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * sequence_length * llm.embeded_length * elem_size

    moe_share_add_route                = Activation()
    moe_share_add_route.dtype          = llm.dtype
    moe_share_add_route.algo           = 'none'
    moe_share_add_route.m_size         = work.batch_size * sequence_length
    moe_share_add_route.n_size         = llm.embeded_length
    moe_share_add_route.gate_enable    = 0
    moe_share_add_route.bias_enable    = 1
    moe_share_add_route.acti_numer     = work.numerical_check_enable

    kernel_flow["moe_share_add_route"] = {
        "type"                          : "addi",
        "input"                         : {"on": "spaceA",    "name": "moe_shared_0_down"},
        "bias"                          : {"on": "spaceA",    "name": "moe_route_output"},
        "output"                        : {"on": "spaceA",    "name": "moe_o"},
        "cfg"                           : moe_share_add_route
    }

    ####################################
    #       21. ResNet Addition        #
    ####################################

    moe_resnet                          = Activation()
    moe_resnet.dtype                    = llm.dtype
    moe_resnet.algo                     = 'none'
    moe_resnet.m_size                   = work.batch_size * sequence_length
    moe_resnet.n_size                   = llm.embeded_length
    moe_resnet.gate_enable              = 0
    moe_resnet.bias_enable              = 1
    moe_resnet.acti_numer               = work.numerical_check_enable

    kernel_flow["moe_resnet"] = {
        "type"                          : "addi",
        "input"                         : {"on": "spaceB",   "name": "layer_input"},
        "bias"                          : {"on": "spaceA",    "name": "moe_o"},
        "output"                        : {"on": "spaceB",   "name": "layer_input"},
        "cfg"                           : moe_resnet
    }

    spaceA_hbm_plan, spaceB_hbm_plan = reoffset_hbm_plans(arch, spaceA_hbm_plan, spaceB_hbm_plan)
    if gen_c2c_flow:
        return kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan, c2c_flow
    else:
        return kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan
        pass
    pass
