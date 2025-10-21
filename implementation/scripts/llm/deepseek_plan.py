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


def deepseek_decode_layer_plan(llm, work, arch):

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
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.embeded_length),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.embeded_length),
        "size"                          : work.batch_size * work.speculative_factor * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * work.speculative_factor * llm.embeded_length * elem_size


    south_hbm_plan["cn_caches"] = {
        "addr"                          : south_hbm_addr,
        "view"                          :(work.batch_size,  llm.max_sequence_length,  llm.kv_lora_rank),
        "shape"                         :(work.batch_size * llm.max_sequence_length,  llm.kv_lora_rank),
        "size"                          : work.batch_size * llm.max_sequence_length * llm.kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * llm.max_sequence_length * llm.kv_lora_rank * elem_size

    south_hbm_plan["cr_caches"] = {
        "addr"                          : south_hbm_addr,
        "view"                          :(work.batch_size,  llm.max_sequence_length,  llm.rope_head_dim),
        "shape"                         :(work.batch_size * llm.max_sequence_length,  llm.rope_head_dim),
        "size"                          : work.batch_size * llm.max_sequence_length * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * llm.max_sequence_length * llm.rope_head_dim * elem_size

    west_hbm_plan["position"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor),
        "shape"                         :(work.batch_size * work.speculative_factor,),
        "size"                          : work.batch_size * work.speculative_factor * index_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * index_size
    west_hbm_addr                       = align_addr(west_hbm_addr)


    #################################
    #       1. Normalization        #
    #################################

    west_hbm_plan["attn_norm"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.embeded_length),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.embeded_length),
        "size"                          : work.batch_size * work.speculative_factor * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * llm.embeded_length * elem_size

    attn_norm_cfg                       = RMSNorm()
    attn_norm_cfg.dtype                 = llm.dtype
    attn_norm_cfg.m_size                = work.batch_size * work.speculative_factor
    attn_norm_cfg.n_size                = llm.embeded_length
    attn_norm_cfg.norm_numer            = work.numerical_check_enable

    kernel_flow["attn_norm"] = {
        "type"                          : "norm",
        "input"                         : {"on": "south",   "name": "layer_input"},
        "output"                        : {"on": "west",    "name": "attn_norm"},
        "cfg"                           : attn_norm_cfg
    }


    #########################################################
    #       2. Latent Projection (C^Q, C^KV merged)         #
    #########################################################

    south_hbm_plan["latent_qc_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.embeded_length,  (llm.q_lora_rank + llm.kv_lora_rank)),
        "size"                          : llm.embeded_length * (llm.q_lora_rank + llm.kv_lora_rank) * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * (llm.q_lora_rank + llm.kv_lora_rank) * elem_size

    west_hbm_plan["attn_latqc"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  (llm.q_lora_rank + llm.kv_lora_rank)),
        "shape"                         :(work.batch_size * work.speculative_factor,  (llm.q_lora_rank + llm.kv_lora_rank)),
        "size"                          : work.batch_size * work.speculative_factor * (llm.q_lora_rank + llm.kv_lora_rank) * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * (llm.q_lora_rank + llm.kv_lora_rank) * elem_size

    attn_latqc_proj                     = SummaGEMM()
    attn_latqc_proj.dtype               = llm.dtype
    attn_latqc_proj.m_size              = work.batch_size * work.speculative_factor
    attn_latqc_proj.n_size              = llm.q_lora_rank + llm.kv_lora_rank
    attn_latqc_proj.k_size              = llm.embeded_length
    attn_latqc_proj.resha_x_from_enable = 0
    attn_latqc_proj.resha_z_to_enable   = 0
    attn_latqc_proj.summa_numer         = work.numerical_check_enable

    kernel_flow["attn_latqc_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "west",    "name": "attn_norm"},
        "weight"                        : {"on": "south",   "name": "latent_qc_weight"},
        "output"                        : {"on": "west",    "name": "attn_latqc"},
        "cfg"                           : attn_latqc_proj
    }

    #########################################
    #       3. Latent Projection CR         #
    #########################################

    south_hbm_plan["latent_cr_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.embeded_length,  llm.rope_head_dim),
        "size"                          : llm.embeded_length * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.embeded_length * llm.rope_head_dim * elem_size


    south_hbm_plan["attn_latcr"] = {
        "addr"                          : south_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.rope_head_dim),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.rope_head_dim),
        "size"                          : work.batch_size * work.speculative_factor * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += work.batch_size * work.speculative_factor * llm.rope_head_dim * elem_size

    attn_latcr_proj                     = SummaGEMM()
    attn_latcr_proj.dtype               = llm.dtype
    attn_latcr_proj.m_size              = work.batch_size * work.speculative_factor
    attn_latcr_proj.n_size              = llm.rope_head_dim
    attn_latcr_proj.k_size              = llm.embeded_length
    attn_latcr_proj.resha_x_from_enable = 0
    attn_latcr_proj.resha_z_to_enable   = 0
    attn_latcr_proj.summa_numer         = work.numerical_check_enable

    kernel_flow["attn_latcr_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "west",    "name": "attn_norm"},
        "weight"                        : {"on": "south",   "name": "latent_cr_weight"},
        "output"                        : {"on": "south",   "name": "attn_latcr"},
        "cfg"                           : attn_latcr_proj
    }

    ############################
    #       4. CR RoPE         #
    ############################

    west_hbm_plan["rope_c_cos_table"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.rope_head_dim // 2)),
        "size"                          : llm.max_sequence_length * (llm.rope_head_dim // 2) * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                      += llm.max_sequence_length * (llm.rope_head_dim // 2) * elem_size

    west_hbm_plan["rope_c_sin_table"] = {
        "addr"                          : west_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.rope_head_dim // 2)),
        "size"                          : llm.max_sequence_length * (llm.rope_head_dim // 2) * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                      += llm.max_sequence_length * (llm.rope_head_dim // 2) * elem_size

    attn_rope_cr                        = RoPE()
    attn_rope_cr.dtype                  = llm.dtype
    attn_rope_cr.m_size                 = work.batch_size * work.speculative_factor
    attn_rope_cr.n_size                 = llm.rope_head_dim
    attn_rope_cr.maximun_seqlen         = llm.max_sequence_length
    attn_rope_cr.view_enable            = 0
    attn_rope_cr.rope_numer             = work.numerical_check_enable

    kernel_flow["attn_rope_cr"] = {
        "type"                          : "rope",
        "input"                         : {"on": "south",  "name": "attn_latcr"},
        "output"                        : {"on": "south",  "name": "attn_latcr"},
        "cos"                           : {"on": "west",   "name": "rope_c_cos_table"},
        "sin"                           : {"on": "west",   "name": "rope_c_sin_table"},
        "position"                      : {"on": "west",   "name": "position"},
        "cfg"                           : attn_rope_cr
    }


    ####################################
    #       5. Layout Remaping         #
    ####################################

    west_hbm_plan["attn_latq"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.q_lora_rank),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.q_lora_rank),
        "size"                          : work.batch_size * work.speculative_factor * llm.q_lora_rank * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * llm.q_lora_rank * elem_size

    kernel_flow["attn_cache_concat"] = {
        "type"                          : "layout",
        "input1"                        : {"on": "west",    "name": "attn_latqc"},
        "input2"                        : {"on": "south",   "name": "attn_latcr"},
        "output1"                       : {"on": "west",    "name": "attn_latq"},
        "output2"                       : {"on": "south",   "name": "cn_caches", "sequence_offset": work.kv_cache_length},
        "output3"                       : {"on": "south",   "name": "cr_caches", "sequence_offset": work.kv_cache_length},
        "cfg"                           : None
    }


    ##################################
    #       6. QN Projection         #
    ##################################

    south_hbm_plan["qn_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.q_lora_rank,  llm.num_heads * llm.kv_lora_rank),
        "size"                          : llm.q_lora_rank * llm.num_heads * llm.kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.q_lora_rank * llm.num_heads * llm.kv_lora_rank * elem_size

    west_hbm_plan["attn_qn"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.num_heads,  llm.kv_lora_rank),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.num_heads * llm.kv_lora_rank),
        "size"                          : work.batch_size * work.speculative_factor * llm.num_heads * llm.kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * llm.num_heads * llm.kv_lora_rank * elem_size

    attn_qn_proj                        = SummaGEMM()
    attn_qn_proj.dtype                  = llm.dtype
    attn_qn_proj.m_size                 = work.batch_size * work.speculative_factor
    attn_qn_proj.n_size                 = llm.num_heads * llm.kv_lora_rank
    attn_qn_proj.k_size                 = llm.q_lora_rank
    attn_qn_proj.resha_x_from_enable    = 0
    attn_qn_proj.resha_z_to_enable      = 0
    attn_qn_proj.summa_numer            = work.numerical_check_enable

    kernel_flow["attn_qn_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "west",    "name": "attn_latq"},
        "weight"                        : {"on": "south",   "name": "qn_proj_weight"},
        "output"                        : {"on": "west",    "name": "attn_qn"},
        "cfg"                           : attn_qn_proj
    }

    ##################################
    #       7. QR Projection         #
    ##################################

    south_hbm_plan["qr_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.q_lora_rank,  llm.num_heads * llm.rope_head_dim),
        "size"                          : llm.q_lora_rank * llm.num_heads * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.q_lora_rank * llm.num_heads * llm.rope_head_dim * elem_size

    west_hbm_plan["attn_qr"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.num_heads,  llm.rope_head_dim),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.num_heads * llm.rope_head_dim),
        "size"                          : work.batch_size * work.speculative_factor * llm.num_heads * llm.rope_head_dim * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * llm.num_heads * llm.rope_head_dim * elem_size

    attn_qr_proj                        = SummaGEMM()
    attn_qr_proj.dtype                  = llm.dtype
    attn_qr_proj.m_size                 = work.batch_size * work.speculative_factor
    attn_qr_proj.n_size                 = llm.num_heads * llm.rope_head_dim
    attn_qr_proj.k_size                 = llm.q_lora_rank
    attn_qr_proj.resha_x_from_enable    = 0
    attn_qr_proj.resha_z_to_enable      = 0
    attn_qr_proj.summa_numer            = work.numerical_check_enable

    kernel_flow["attn_qr_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "west",    "name": "attn_latq"},
        "weight"                        : {"on": "south",   "name": "qr_proj_weight"},
        "output"                        : {"on": "west",    "name": "attn_qr"},
        "cfg"                           : attn_qr_proj
    }

    ############################
    #       8. QR RoPE         #
    ############################

    south_hbm_plan["rope_q_cos_table"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.num_heads * llm.rope_head_dim // 2)),
        "size"                          : llm.max_sequence_length * (llm.num_heads * llm.rope_head_dim // 2) * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.max_sequence_length * (llm.num_heads * llm.rope_head_dim // 2) * elem_size

    south_hbm_plan["rope_q_sin_table"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.max_sequence_length,  (llm.num_heads * llm.rope_head_dim // 2)),
        "size"                          : llm.max_sequence_length * (llm.num_heads * llm.rope_head_dim // 2) * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.max_sequence_length * (llm.num_heads * llm.rope_head_dim // 2) * elem_size

    attn_rope_qr                        = RoPE()
    attn_rope_qr.dtype                  = llm.dtype
    attn_rope_qr.m_size                 = work.batch_size * work.speculative_factor
    attn_rope_qr.n_size                 = llm.num_heads * llm.rope_head_dim
    attn_rope_qr.maximun_seqlen         = llm.max_sequence_length
    attn_rope_qr.view_enable            = 0
    attn_rope_qr.rope_numer             = work.numerical_check_enable

    kernel_flow["attn_rope_qr"] = {
        "type"                          : "rope",
        "input"                         : {"on": "west",   "name": "attn_qr"},
        "output"                        : {"on": "west",   "name": "attn_qr"},
        "cos"                           : {"on": "south",  "name": "rope_q_cos_table"},
        "sin"                           : {"on": "south",  "name": "rope_q_sin_table"},
        "position"                      : {"on": "west",   "name": "position"},
        "cfg"                           : attn_rope_qr
    }

    ############################
    #       9. FlatMLA         #
    ############################

    west_hbm_plan["attn_o"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.num_heads,  llm.kv_lora_rank),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.num_heads * llm.kv_lora_rank),
        "size"                          : work.batch_size * work.speculative_factor * llm.num_heads * llm.kv_lora_rank * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * llm.num_heads * llm.kv_lora_rank * elem_size

    attn_flatmla                        = FlatMLA()
    attn_flatmla.dtype                  = llm.dtype
    attn_flatmla.kv_sequence_length     = work.kv_cache_length + work.speculative_factor
    attn_flatmla.q_sequence_length      = 1
    attn_flatmla.speculative_length     = work.speculative_factor
    attn_flatmla.nope_head_dim          = llm.kv_lora_rank
    attn_flatmla.rope_head_dim          = llm.rope_head_dim
    attn_flatmla.num_head               = llm.num_heads
    attn_flatmla.batch_size             = work.batch_size
    attn_flatmla.flatten_numer          = work.numerical_check_enable

    kernel_flow["attn_flatmla"] = {
        "type"                          : "flatmla",
        "qn"                            : {"on": "west",   "name": "attn_qn"},
        "qr"                            : {"on": "west",   "name": "attn_qr"},
        "cn"                            : {"on": "south",  "name": "cn_caches"},
        "cr"                            : {"on": "south",  "name": "cr_caches"},
        "o"                             : {"on": "west",   "name": "attn_o"},
        "cfg"                           : attn_flatmla
    }

    #############################################
    #       10. O First Down Projection         #
    #############################################

    south_hbm_plan["o1_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.kv_lora_rank,  llm.num_heads * llm.head_dimension),
        "size"                          : llm.kv_lora_rank * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.kv_lora_rank * llm.num_heads * llm.head_dimension * elem_size

    west_hbm_plan["attn_o1"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.num_heads,  llm.head_dimension),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.num_heads * llm.head_dimension),
        "size"                          : work.batch_size * work.speculative_factor * llm.num_heads * llm.head_dimension * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * llm.num_heads * llm.head_dimension * elem_size

    kernel_flow["attn_o1_proj"] = {
        "type"                          : "ofdp",
        "input"                         : {"on": "west",    "name": "attn_o"},
        "weight"                        : {"on": "south",   "name": "o1_proj_weight"},
        "output"                        : {"on": "west",    "name": "attn_o1"},
        "cfg"                           : None
    }

    #############################################
    #       11. O Final Down Projection         #
    #############################################

    south_hbm_plan["o2_proj_weight"] = {
        "addr"                          : south_hbm_addr,
        "shape"                         :(llm.num_heads * llm.head_dimension,  llm.embeded_length),
        "size"                          : llm.num_heads * llm.head_dimension * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    south_hbm_addr                      += llm.num_heads * llm.head_dimension * llm.embeded_length * elem_size

    west_hbm_plan["attn_o2"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor,  llm.embeded_length),
        "shape"                         :(work.batch_size * work.speculative_factor,  llm.embeded_length),
        "size"                          : work.batch_size * work.speculative_factor * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * llm.embeded_length * elem_size

    attn_o2_proj                        = SummaGEMM()
    attn_o2_proj.dtype                  = llm.dtype
    attn_o2_proj.m_size                 = work.batch_size * work.speculative_factor
    attn_o2_proj.n_size                 = llm.embeded_length
    attn_o2_proj.k_size                 = llm.num_heads * llm.head_dimension
    attn_o2_proj.resha_x_from_enable    = 0
    attn_o2_proj.resha_z_to_enable      = 0
    attn_o2_proj.summa_numer            = work.numerical_check_enable

    kernel_flow["attn_o2_proj"] = {
        "type"                          : "gemm",
        "input"                         : {"on": "west",    "name": "attn_o1"},
        "weight"                        : {"on": "south",   "name": "o2_proj_weight"},
        "output"                        : {"on": "west",    "name": "attn_o2"},
        "cfg"                           : attn_o2_proj
    }

    ##################################
    #       12. ResNet Addition      #
    ##################################
    attn_resnet                         = Activation()
    attn_resnet.dtype                   = llm.dtype
    attn_resnet.algo                    = 'none'
    attn_resnet.m_size                  = work.batch_size * work.speculative_factor
    attn_resnet.n_size                  = llm.embeded_length
    attn_resnet.gate_enable             = 0
    attn_resnet.bias_enable             = 1
    attn_resnet.acti_numer              = work.numerical_check_enable

    kernel_flow["attn_resnet"] = {
        "type"                          : "addi",
        "input"                         : {"on": "south",   "name": "layer_input"},
        "bias"                          : {"on": "west",    "name": "attn_o2"},
        "output"                        : {"on": "south",   "name": "layer_input"},
        "cfg"                           : attn_resnet
    }

    ##################################
    #       13. Normalization        #
    ##################################
    west_hbm_plan["ffn_norm"] = {
        "addr"                          : west_hbm_addr,
        "view"                          :(work.batch_size,  work.speculative_factor, llm.embeded_length),
        "shape"                         :(work.batch_size * work.speculative_factor, llm.embeded_length),
        "size"                          : work.batch_size * work.speculative_factor * llm.embeded_length * elem_size,
        "tensor"                        : None
    }
    west_hbm_addr                       += work.batch_size * work.speculative_factor * llm.embeded_length * elem_size

    ffn_norm_cfg                        = RMSNorm()
    ffn_norm_cfg.dtype                  = llm.dtype
    ffn_norm_cfg.m_size                 = work.batch_size * work.speculative_factor
    ffn_norm_cfg.n_size                 = llm.embeded_length
    ffn_norm_cfg.norm_numer             = work.numerical_check_enable

    kernel_flow["ffn_norm"] = {
        "type"                          : "norm",
        "input"                         : {"on": "south",   "name": "layer_input"},
        "output"                        : {"on": "west",    "name": "ffn_norm"},
        "cfg"                           : ffn_norm_cfg
    }

    return kernel_flow, west_hbm_plan, south_hbm_plan
    pass
