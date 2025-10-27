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
import shutil
import numpy as np
import importlib.util
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

def kernel_flow_simplify(kernel_flow):
    return kernel_flow
    pass

def hbm_plan_summary(plan):
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

def normal_llm_prefill_layer_plan(llm, work, arch):

    #Basic Settings
    elem_size                           = 1 if llm.dtype == 'fp8' else 2
    index_size                          = 4 #uint32_t
    kernel_flow                         = {}
    spaceA_hbm_plan                     = {}
    spaceB_hbm_plan                     = {}
    spaceA_hbm_addr                     = 0
    spaceB_hbm_addr                     = 0

    spaceB_hbm_plan["layer_input"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

    spaceA_hbm_plan["position"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,),
        "size"                          : work.batch_size * work.prefill_input_token * index_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * index_size
    spaceA_hbm_addr                       = align_addr(spaceA_hbm_addr)



    #################################
    #       1. Normalization        #
    #################################
    spaceA_hbm_plan["attn_norm"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

    attn_norm_cfg                       = RMSNorm()
    attn_norm_cfg.dtype                 = llm.dtype
    attn_norm_cfg.m_size                = work.batch_size * work.prefill_input_token
    attn_norm_cfg.n_size                = llm.embeded_length
    attn_norm_cfg.norm_numer            = work.numerical_check_enable

    kernel_flow["attn_norm"] = {
        "type"                          : "norm",
        "input"                         : {"on": "spaceB",   "name": "layer_input"},
        "output"                        : {"on": "spaceA",    "name": "attn_norm"},
        "cfg"                           : attn_norm_cfg
    }


    #################################
    #       2. Q Projection         #
    #################################
    spaceB_hbm_plan["q_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.num_heads * llm.head_dimension),
        "size"                          : llm.embeded_length * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.embeded_length * llm.num_heads * llm.head_dimension * elem_size

    spaceA_hbm_plan["attn_q"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size * llm.num_heads,  work.prefill_input_token,  llm.head_dimension),
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.num_heads,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size

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
        "input"                         : {"on": "spaceA",    "name": "attn_norm"},
        "weight"                        : {"on": "spaceB",   "name": "q_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "attn_q"},
        "cfg"                           : attn_q_proj
    }



    #################################
    #       3. KV Projection        #
    #################################
    spaceB_hbm_plan["k_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.head_groups * llm.head_dimension),
        "size"                          : llm.embeded_length * llm.head_groups * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.embeded_length * llm.head_groups * llm.head_dimension * elem_size

    spaceB_hbm_plan["attn_k"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(work.batch_size * llm.head_groups,  work.prefill_input_token,  llm.head_dimension),
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.head_groups,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.head_groups * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += work.batch_size * work.prefill_input_token * llm.head_groups * llm.head_dimension * elem_size

    attn_k_proj                         = SummaGEMM()
    attn_k_proj.dtype                   = llm.dtype
    attn_k_proj.m_size                  = work.batch_size * work.prefill_input_token
    attn_k_proj.n_size                  = llm.head_groups * llm.head_dimension
    attn_k_proj.k_size                  = llm.embeded_length
    attn_k_proj.resha_x_from_enable     = 0
    attn_k_proj.resha_z_to_enable       = 1
    attn_k_proj.resha_z_to_m            = work.batch_size * work.prefill_input_token * llm.head_groups
    attn_k_proj.summa_numer             = work.numerical_check_enable

    kernel_flow["attn_k_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "attn_norm"},
        "weight"                        : {"on": "spaceB",   "name": "k_proj_weight"},
        "output"                        : {"on": "spaceB",   "name": "attn_k"},
        "cfg"                           : attn_k_proj
    }


    spaceB_hbm_plan["v_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.head_groups * llm.head_dimension),
        "size"                          : llm.embeded_length * llm.head_groups * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.embeded_length * llm.head_groups * llm.head_dimension * elem_size

    spaceB_hbm_plan["attn_v"] = {
        "addr"                          : spaceB_hbm_addr,
        "view"                          :(work.batch_size * llm.head_groups,  work.prefill_input_token,  llm.head_dimension),
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.head_groups,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.head_groups * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += work.batch_size * work.prefill_input_token * llm.head_groups * llm.head_dimension * elem_size

    attn_v_proj                         = SummaGEMM()
    attn_v_proj.dtype                   = llm.dtype
    attn_v_proj.m_size                  = work.batch_size * work.prefill_input_token
    attn_v_proj.n_size                  = llm.head_groups * llm.head_dimension
    attn_v_proj.k_size                  = llm.embeded_length
    attn_v_proj.resha_x_from_enable     = 0
    attn_v_proj.resha_z_to_enable       = 1
    attn_v_proj.resha_z_to_m            = work.batch_size * work.prefill_input_token * llm.head_groups
    attn_v_proj.summa_numer             = work.numerical_check_enable

    kernel_flow["attn_v_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "attn_norm"},
        "weight"                        : {"on": "spaceB",   "name": "v_proj_weight"},
        "output"                        : {"on": "spaceB",   "name": "attn_v"},
        "cfg"                           : attn_v_proj
    }


    #########################
    #       4. QK RoPE      #
    #########################
    if llm.qk_rope_enable:
        ## Cosine and Sine Table
        spaceB_hbm_plan["rope_q_cos_table"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.num_heads * llm.head_dimension // 2)),
        "size"                          : llm.max_sequence_length * (llm.num_heads * llm.head_dimension // 2) * elem_size,
        "tensor"                        : None
        }
        spaceB_hbm_addr                  += llm.max_sequence_length * (llm.num_heads * llm.head_dimension // 2) * elem_size

        spaceB_hbm_plan["rope_q_sin_table"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.num_heads * llm.head_dimension // 2)),
        "size"                          : llm.max_sequence_length * (llm.num_heads * llm.head_dimension // 2) * elem_size,
        "tensor"                        : None
        }
        spaceB_hbm_addr                  += llm.max_sequence_length * (llm.num_heads * llm.head_dimension // 2) * elem_size

        attn_rope_q                     = RoPE()
        attn_rope_q.dtype               = llm.dtype
        attn_rope_q.m_size              = work.batch_size * work.prefill_input_token
        attn_rope_q.n_size              = llm.num_heads * llm.head_dimension
        attn_rope_q.maximun_seqlen      = llm.max_sequence_length
        attn_rope_q.view_enable         = 1
        attn_rope_q.view_n              = llm.head_dimension
        attn_rope_q.rope_numer          = work.numerical_check_enable

        kernel_flow["attn_rope_q"] = {
        "type"                          : "rope",
        "input"                         : {"on": "spaceA",   "name": "attn_q"},
        "output"                        : {"on": "spaceA",   "name": "attn_q"},
        "cos"                           : {"on": "spaceB",  "name": "rope_q_cos_table"},
        "sin"                           : {"on": "spaceB",  "name": "rope_q_sin_table"},
        "position"                      : {"on": "spaceA",   "name": "position"},
        "cfg"                           : attn_rope_q
        }

        spaceA_hbm_plan["rope_k_cos_table"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.head_groups * llm.head_dimension // 2)),
        "size"                          : llm.max_sequence_length * (llm.head_groups * llm.head_dimension // 2) * elem_size,
        "tensor"                        : None
        }
        spaceA_hbm_addr                   += llm.max_sequence_length * (llm.head_groups * llm.head_dimension // 2) * elem_size

        spaceA_hbm_plan["rope_k_sin_table"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.head_groups * llm.head_dimension // 2)),
        "size"                          : llm.max_sequence_length * (llm.head_groups * llm.head_dimension // 2) * elem_size,
        "tensor"                        : None
        }
        spaceA_hbm_addr                   += llm.max_sequence_length * (llm.head_groups * llm.head_dimension // 2) * elem_size

        attn_rope_k                     = RoPE()
        attn_rope_k.dtype               = llm.dtype
        attn_rope_k.m_size              = work.batch_size * work.prefill_input_token
        attn_rope_k.n_size              = llm.head_groups * llm.head_dimension
        attn_rope_k.maximun_seqlen      = llm.max_sequence_length
        attn_rope_k.view_enable         = 1
        attn_rope_k.view_n              = llm.head_dimension
        attn_rope_k.rope_numer          = work.numerical_check_enable

        kernel_flow["attn_rope_k"] = {
        "type"                          : "rope",
        "input"                         : {"on": "spaceB",  "name": "attn_k"},
        "output"                        : {"on": "spaceB",  "name": "attn_k"},
        "cos"                           : {"on": "spaceA",   "name": "rope_k_cos_table"},
        "sin"                           : {"on": "spaceA",   "name": "rope_k_sin_table"},
        "position"                      : {"on": "spaceA",   "name": "position"},
        "cfg"                           : attn_rope_k
        }
        pass

    #############################
    #       5. Attention        #
    #############################
    spaceA_hbm_plan["attn_o"] = {
        "addr"                          : spaceA_hbm_addr,
        "view"                          :(work.batch_size * llm.num_heads,  work.prefill_input_token,  llm.head_dimension),
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.num_heads,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size

    attn_mha                            = FlatAttetion()
    attn_mha.dtype                      = llm.dtype
    attn_mha.kv_sequence_length         = work.prefill_input_token
    attn_mha.q_sequence_length          = work.prefill_input_token
    attn_mha.speculative_length         = 1 # Prefill process here
    attn_mha.head_dimemsion             = llm.head_dimension
    attn_mha.num_head                   = llm.num_heads
    attn_mha.num_head_group             = llm.head_groups
    attn_mha.batch_size                 = work.batch_size
    attn_mha.flatten_async              = 1 # Best Performance
    attn_mha.flatten_numer              = work.numerical_check_enable

    kernel_flow["attn_mha"] = {
        "type"                          : "flat_attn",
        "q"                             : {"on": "spaceA",    "name": "attn_q"},
        "k"                             : {"on": "spaceB",   "name": "attn_k"},
        "v"                             : {"on": "spaceB",   "name": "attn_v"},
        "o"                             : {"on": "spaceA",    "name": "attn_o"},
        "cfg"                           : attn_mha
    }


    #############################
    #       6. O Projection     #
    #############################
    spaceB_hbm_plan["o_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.num_heads * llm.head_dimension,  llm.embeded_length),
        "size"                          : llm.num_heads * llm.head_dimension * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.num_heads * llm.head_dimension * llm.embeded_length * elem_size

    spaceA_hbm_plan["attn_a"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

    attn_o_proj                         = SummaGEMM()
    attn_o_proj.dtype                   = llm.dtype
    attn_o_proj.m_size                  = work.batch_size * work.prefill_input_token
    attn_o_proj.n_size                  = llm.embeded_length
    attn_o_proj.k_size                  = llm.num_heads * llm.head_dimension
    attn_o_proj.resha_x_from_enable     = 1
    attn_o_proj.resha_z_to_enable       = 0
    attn_o_proj.resha_x_from_m          = work.batch_size * work.prefill_input_token * llm.num_heads
    attn_o_proj.summa_numer             = work.numerical_check_enable

    kernel_flow["attn_o_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "attn_o"},
        "weight"                        : {"on": "spaceB",   "name": "o_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "attn_a"},
        "cfg"                           : attn_o_proj
    }

    #################################
    #       7. ResNet Addition      #
    #################################
    attn_resnet                         = Activation()
    attn_resnet.dtype                   = llm.dtype
    attn_resnet.algo                    = 'none'
    attn_resnet.m_size                  = work.batch_size * work.prefill_input_token
    attn_resnet.n_size                  = llm.embeded_length
    attn_resnet.gate_enable             = 0
    attn_resnet.bias_enable             = 1
    attn_resnet.acti_numer              = work.numerical_check_enable

    kernel_flow["attn_resnet"] = {
        "type"                          : "addi",
        "input"                         : {"on": "spaceB",   "name": "layer_input"},
        "bias"                          : {"on": "spaceA",    "name": "attn_a"},
        "output"                        : {"on": "spaceB",   "name": "layer_input"},
        "cfg"                           : attn_resnet
    }

    #################################
    #       8. Normalization        #
    #################################
    spaceA_hbm_plan["ffn_norm"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

    ffn_norm_cfg                        = RMSNorm()
    ffn_norm_cfg.dtype                  = llm.dtype
    ffn_norm_cfg.m_size                 = work.batch_size * work.prefill_input_token
    ffn_norm_cfg.n_size                 = llm.embeded_length
    ffn_norm_cfg.norm_numer             = work.numerical_check_enable

    kernel_flow["ffn_norm"] = {
        "type"                          : "norm",
        "input"                         : {"on": "spaceB",   "name": "layer_input"},
        "output"                        : {"on": "spaceA",    "name": "ffn_norm"},
        "cfg"                           : ffn_norm_cfg
    }

    #####################################
    #       9. FFN Up Projection        #
    #####################################
    spaceB_hbm_plan["up_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.mlp_inter_dim),
        "size"                          : llm.embeded_length * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.embeded_length * llm.mlp_inter_dim * elem_size

    spaceA_hbm_plan["ffn_up"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,  llm.mlp_inter_dim),
        "size"                          : work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size

    ffn_up_proj                         = SummaGEMM()
    ffn_up_proj.dtype                   = llm.dtype
    ffn_up_proj.m_size                  = work.batch_size * work.prefill_input_token
    ffn_up_proj.n_size                  = llm.mlp_inter_dim
    ffn_up_proj.k_size                  = llm.embeded_length
    ffn_up_proj.resha_x_from_enable     = 0
    ffn_up_proj.resha_z_to_enable       = 0
    ffn_up_proj.summa_numer             = work.numerical_check_enable

    kernel_flow["ffn_up_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "ffn_norm"},
        "weight"                        : {"on": "spaceB",   "name": "up_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "ffn_up"},
        "cfg"                           : ffn_up_proj
    }

    #####################################
    #       10. FFN Gate Projection     #
    #####################################
    spaceB_hbm_plan["gate_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.mlp_inter_dim),
        "size"                          : llm.embeded_length * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.embeded_length * llm.mlp_inter_dim * elem_size

    spaceA_hbm_plan["ffn_gate"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,  llm.mlp_inter_dim),
        "size"                          : work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size

    ffn_gate_proj                       = SummaGEMM()
    ffn_gate_proj.dtype                 = llm.dtype
    ffn_gate_proj.m_size                = work.batch_size * work.prefill_input_token
    ffn_gate_proj.n_size                = llm.mlp_inter_dim
    ffn_gate_proj.k_size                = llm.embeded_length
    ffn_gate_proj.resha_x_from_enable   = 0
    ffn_gate_proj.resha_z_to_enable     = 0
    ffn_gate_proj.summa_numer           = work.numerical_check_enable

    kernel_flow["ffn_gate_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "ffn_norm"},
        "weight"                        : {"on": "spaceB",   "name": "gate_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "ffn_gate"},
        "cfg"                           : ffn_gate_proj
    }

    #################################
    #       11. FFN Activation      #
    #################################
    spaceA_hbm_plan["ffn_acti"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,  llm.mlp_inter_dim),
        "size"                          : work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size

    if llm.mlp_acti_bias_enable:
        spaceB_hbm_plan["ffn_acti_bias"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,  llm.mlp_inter_dim),
        "size"                          : work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
        }
        spaceB_hbm_addr                  += work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size
        pass

    ffn_acti                            = Activation()
    ffn_acti.dtype                      = llm.dtype
    ffn_acti.algo                       = llm.mlp_acti_algo
    ffn_acti.m_size                     = work.batch_size * work.prefill_input_token
    ffn_acti.n_size                     = llm.mlp_inter_dim
    ffn_acti.gate_enable                = 1
    ffn_acti.bias_enable                = llm.mlp_acti_bias_enable
    ffn_acti.acti_numer                 = work.numerical_check_enable

    kernel_flow["ffn_acti"] = {
        "type"                          : "acti",
        "input"                         : {"on": "spaceA",    "name": "ffn_up"},
        "gate"                          : {"on": "spaceA",    "name": "ffn_gate"},
        "output"                        : {"on": "spaceA",    "name": "ffn_acti"},
        "cfg"                           : ffn_acti
    }

    if llm.mlp_acti_bias_enable:
        kernel_flow["ffn_acti"]["bias"] = {"on": "spaceB",   "name": "ffn_acti_bias"}
        pass

    #####################################
    #       12. FFN Down Projection     #
    #####################################
    spaceB_hbm_plan["down_proj_weight"] = {
        "addr"                          : spaceB_hbm_addr,
        "shape"                         :(llm.mlp_inter_dim,  llm.embeded_length),
        "size"                          : llm.mlp_inter_dim * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceB_hbm_addr                      += llm.mlp_inter_dim * llm.embeded_length * elem_size

    spaceA_hbm_plan["ffn_o"] = {
        "addr"                          : spaceA_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    spaceA_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

    ffn_down_proj                       = SummaGEMM()
    ffn_down_proj.dtype                 = llm.dtype
    ffn_down_proj.m_size                = work.batch_size * work.prefill_input_token
    ffn_down_proj.n_size                = llm.embeded_length
    ffn_down_proj.k_size                = llm.mlp_inter_dim
    ffn_down_proj.resha_x_from_enable   = 0
    ffn_down_proj.resha_z_to_enable     = 0
    ffn_down_proj.summa_numer           = work.numerical_check_enable

    kernel_flow["ffn_down_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "spaceA",    "name": "ffn_acti"},
        "weight"                        : {"on": "spaceB",   "name": "down_proj_weight"},
        "output"                        : {"on": "spaceA",    "name": "ffn_o"},
        "cfg"                           : ffn_down_proj
    }

    #################################
    #       13. ResNet Addition     #
    #################################
    ffn_resnet                          = Activation()
    ffn_resnet.dtype                    = llm.dtype
    ffn_resnet.algo                     = 'none'
    ffn_resnet.m_size                   = work.batch_size * work.prefill_input_token
    ffn_resnet.n_size                   = llm.embeded_length
    ffn_resnet.gate_enable              = 0
    ffn_resnet.bias_enable              = 1
    ffn_resnet.acti_numer               = work.numerical_check_enable

    kernel_flow["ffn_resnet"] = {
        "type"                          : "addi",
        "input"                         : {"on": "spaceB",   "name": "layer_input"},
        "bias"                          : {"on": "spaceA",    "name": "ffn_o"},
        "output"                        : {"on": "spaceB",   "name": "layer_input"},
        "cfg"                           : ffn_resnet
    }
    
    spaceA_hbm_plan, spaceB_hbm_plan = reoffset_hbm_plans(arch, spaceA_hbm_plan, spaceB_hbm_plan)
    return kernel_flow, spaceA_hbm_plan, spaceB_hbm_plan
    pass
