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

def normal_llm_prefill_layer_plan(llm, work, arch):

    #Basic Settings
    elem_size                           = 1 if llm.dtype == 'fp8' else 2
    index_size                          = 4 #uint32_t
    kernel_flow                         = {}
    west_hbm_plan                       = {}
    south_hbm_plan                      = {}
    west_hbm_addr                       = arch.hbm_start_base
    south_hbm_addr                      = arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x

    south_hbm_plan["layer_input"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

    west_hbm_plan["position"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,),
        "size"                          : work.batch_size * work.prefill_input_token * index_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * index_size
    west_hbm_addr                       = align_addr(west_hbm_addr)



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
        "input"                         : {"on": "south",   "name": "layer_input"},
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
        "view"                          :(work.batch_size * llm.num_heads,  work.prefill_input_token,  llm.head_dimension),
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
        "shape"                         :(llm.embeded_length,  llm.head_groups * llm.head_dimension),
        "size"                          : llm.embeded_length * llm.head_groups * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * llm.head_groups * llm.head_dimension * elem_size

    south_hbm_plan["attn_k"] = {
        "addr"                          : south_hbm_addr,
        "view"                          :(work.batch_size * llm.head_groups,  work.prefill_input_token,  llm.head_dimension),
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.head_groups,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.head_groups * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * work.prefill_input_token * llm.head_groups * llm.head_dimension * elem_size

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
        "input"                         : {"on": "west",    "name": "attn_norm"},
        "weight"                        : {"on": "south",   "name": "k_proj_weight"},
        "output"                        : {"on": "south",   "name": "attn_k"},
        "cfg"                           : attn_k_proj
    }


    south_hbm_plan["v_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.head_groups * llm.head_dimension),
        "size"                          : llm.embeded_length * llm.head_groups * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * llm.head_groups * llm.head_dimension * elem_size

    south_hbm_plan["attn_v"] = {
        "addr"                          : south_hbm_addr,
        "view"                          :(work.batch_size * llm.head_groups,  work.prefill_input_token,  llm.head_dimension),
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.head_groups,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.head_groups * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * work.prefill_input_token * llm.head_groups * llm.head_dimension * elem_size

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
        "input"                         : {"on": "west",    "name": "attn_norm"},
        "weight"                        : {"on": "south",   "name": "v_proj_weight"},
        "output"                        : {"on": "south",   "name": "attn_v"},
        "cfg"                           : attn_v_proj
    }


    #########################
    #       4. QK RoPE      #
    #########################
    if llm.qk_rope_enable:
        ## Cosine and Sine Table
        south_hbm_plan["rope_q_cos_table"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.num_heads * llm.head_dimension // 2)),
        "size"                          : llm.max_sequence_length * (llm.num_heads * llm.head_dimension // 2) * elem_size,
        "tensor"                        : None
        }
        south_hbm_addr                  += llm.max_sequence_length * (llm.num_heads * llm.head_dimension // 2) * elem_size

        south_hbm_plan["rope_q_sin_table"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.num_heads * llm.head_dimension // 2)),
        "size"                          : llm.max_sequence_length * (llm.num_heads * llm.head_dimension // 2) * elem_size,
        "tensor"                        : None
        }
        south_hbm_addr                  += llm.max_sequence_length * (llm.num_heads * llm.head_dimension // 2) * elem_size

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
        "input"                         : {"on": "west",   "name": "attn_q"},
        "output"                        : {"on": "west",   "name": "attn_q"},
        "cos"                           : {"on": "south",  "name": "rope_q_cos_table"},
        "sin"                           : {"on": "south",  "name": "rope_q_sin_table"},
        "position"                      : {"on": "west",   "name": "position"},
        "cfg"                           : attn_rope_q
        }

        west_hbm_plan["rope_k_cos_table"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.head_groups * llm.head_dimension // 2)),
        "size"                          : llm.max_sequence_length * (llm.head_groups * llm.head_dimension // 2) * elem_size,
        "tensor"                        : None
        }
        west_hbm_addr                   += llm.max_sequence_length * (llm.head_groups * llm.head_dimension // 2) * elem_size

        west_hbm_plan["rope_k_sin_table"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.head_groups * llm.head_dimension // 2)),
        "size"                          : llm.max_sequence_length * (llm.head_groups * llm.head_dimension // 2) * elem_size,
        "tensor"                        : None
        }
        west_hbm_addr                   += llm.max_sequence_length * (llm.head_groups * llm.head_dimension // 2) * elem_size

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
        "input"                         : {"on": "south",  "name": "attn_k"},
        "output"                        : {"on": "south",  "name": "attn_k"},
        "cos"                           : {"on": "west",   "name": "rope_k_cos_table"},
        "sin"                           : {"on": "west",   "name": "rope_k_sin_table"},
        "position"                      : {"on": "west",   "name": "position"},
        "cfg"                           : attn_rope_k
        }
        pass

    #############################
    #       5. Attention        #
    #############################
    west_hbm_plan["attn_o"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size * llm.num_heads,  work.prefill_input_token,  llm.head_dimension),
        "shape"                         :(work.batch_size * work.prefill_input_token * llm.num_heads,  llm.head_dimension),
        "size"                          : work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.num_heads * llm.head_dimension * elem_size

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
        "q"                             : {"on": "west",    "name": "attn_q"},
        "k"                             : {"on": "south",   "name": "attn_k"},
        "v"                             : {"on": "south",   "name": "attn_v"},
        "o"                             : {"on": "west",    "name": "attn_o"},
        "cfg"                           : attn_mha
    }


    #############################
    #       6. O Projection     #
    #############################
    south_hbm_plan["o_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.num_heads * llm.head_dimension,  llm.embeded_length),
        "size"                          : llm.num_heads * llm.head_dimension * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.num_heads * llm.head_dimension * llm.embeded_length * elem_size

    west_hbm_plan["attn_a"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

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
        "input"                         : {"on": "west",    "name": "attn_o"},
        "weight"                        : {"on": "south",   "name": "o_proj_weight"},
        "output"                        : {"on": "west",    "name": "attn_a"},
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
        "input"                         : {"on": "south",   "name": "layer_input"},
        "bias"                          : {"on": "west",    "name": "attn_a"},
        "output"                        : {"on": "south",   "name": "layer_input"},
        "cfg"                           : attn_resnet
    }

    #################################
    #       8. Normalization        #
    #################################
    west_hbm_plan["ffn_norm"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

    ffn_norm_cfg                        = RMSNorm()
    ffn_norm_cfg.dtype                  = llm.dtype
    ffn_norm_cfg.m_size                 = work.batch_size * work.prefill_input_token
    ffn_norm_cfg.n_size                 = llm.embeded_length
    ffn_norm_cfg.norm_numer             = work.numerical_check_enable

    kernel_flow["ffn_norm"] = {
        "type"                          : "norm",
        "input"                         : {"on": "south",   "name": "layer_input"},
        "output"                        : {"on": "west",    "name": "ffn_norm"},
        "cfg"                           : ffn_norm_cfg
    }

    #####################################
    #       9. FFN Up Projection        #
    #####################################
    south_hbm_plan["up_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.mlp_inter_dim),
        "size"                          : llm.embeded_length * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * llm.mlp_inter_dim * elem_size

    west_hbm_plan["ffn_up"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,  llm.mlp_inter_dim),
        "size"                          : work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size

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
        "input"                         : {"on": "west",    "name": "ffn_norm"},
        "weight"                        : {"on": "south",   "name": "up_proj_weight"},
        "output"                        : {"on": "west",    "name": "ffn_up"},
        "cfg"                           : ffn_up_proj
    }

    #####################################
    #       10. FFN Gate Projection     #
    #####################################
    south_hbm_plan["gate_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.mlp_inter_dim),
        "size"                          : llm.embeded_length * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * llm.mlp_inter_dim * elem_size

    west_hbm_plan["ffn_gate"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,  llm.mlp_inter_dim),
        "size"                          : work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size

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
        "input"                         : {"on": "west",    "name": "ffn_norm"},
        "weight"                        : {"on": "south",   "name": "gate_proj_weight"},
        "output"                        : {"on": "west",    "name": "ffn_gate"},
        "cfg"                           : ffn_gate_proj
    }

    #################################
    #       11. FFN Activation      #
    #################################
    west_hbm_plan["ffn_acti"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,  llm.mlp_inter_dim),
        "size"                          : work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size

    if llm.mlp_acti_bias_enable:
        south_hbm_plan["ffn_acti_bias"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token,  llm.mlp_inter_dim),
        "size"                          : work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size,
        "tensor"                        : None
        }
        south_hbm_addr                  += work.batch_size * work.prefill_input_token * llm.mlp_inter_dim * elem_size
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
        "input"                         : {"on": "west",    "name": "ffn_up"},
        "gate"                          : {"on": "west",    "name": "ffn_gate"},
        "output"                        : {"on": "west",    "name": "ffn_acti"},
        "cfg"                           : ffn_acti
    }

    if llm.mlp_acti_bias_enable:
        kernel_flow["ffn_acti"]["bias"] = {"on": "south",   "name": "ffn_acti_bias"}
        pass

    #####################################
    #       12. FFN Down Projection     #
    #####################################
    south_hbm_plan["down_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.mlp_inter_dim,  llm.embeded_length),
        "size"                          : llm.mlp_inter_dim * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.mlp_inter_dim * llm.embeded_length * elem_size

    west_hbm_plan["ffn_o"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(work.batch_size * work.prefill_input_token, llm.embeded_length),
        "size"                          : work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.prefill_input_token * llm.embeded_length * elem_size

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
        "input"                         : {"on": "west",    "name": "ffn_acti"},
        "weight"                        : {"on": "south",   "name": "down_proj_weight"},
        "output"                        : {"on": "west",    "name": "ffn_o"},
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
        "input"                         : {"on": "south",   "name": "layer_input"},
        "bias"                          : {"on": "west",    "name": "ffn_o"},
        "output"                        : {"on": "south",   "name": "layer_input"},
        "cfg"                           : ffn_resnet
    }
    
    return kernel_flow, west_hbm_plan, south_hbm_plan
    pass
