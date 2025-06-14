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

import sys
import os
import io
import shutil
import numpy as np
import argparse
import importlib.util
from tqdm import tqdm
import preload as pld

class Heads:

    def __init__(self, attn, arch):
        self.attn = attn
        self.arch = arch
        self.d  = self.attn.head_dimemsion
        self.Br = self.attn.flatten_shape
        self.Bc = self.attn.flatten_shape
        self.Tr = self.attn.sequence_length // self.Br
        self.Tc = self.attn.sequence_length // self.Bc
        self.Br_s = self.attn.flatten_shape // self.attn.flatten_scale;
        self.Bc_s = self.attn.flatten_shape // self.attn.flatten_scale;

        self.num_group_x = self.arch.num_cluster_x // self.attn.flatten_scale
        self.num_group_y = self.arch.num_cluster_y // self.attn.flatten_scale
        self.num_head_per_group = (self.attn.batch_size * self.attn.num_head) // (self.num_group_x * self.num_group_y)

        #Initialize heads
        self.Q = []
        self.KT = []
        self.V = []
        self.O = []
        self.O_golden = []

        # Initialize node contianer
        self.Q_node_container = []
        self.KT_node_container = []
        self.V_node_container = []
        self.O_node_container = []
        self.O_golden_node_container = []
        for x in range(self.arch.num_cluster_x):
            self.Q_node_container.append([])
            self.KT_node_container.append([])
            self.V_node_container.append([])
            self.O_node_container.append([])
            self.O_golden_node_container.append([])
            pass

        progress = tqdm(total=self.attn.batch_size * self.attn.num_head, desc="[Emulation] Q_KT_V_O initialization")
        for x in range(self.attn.batch_size * self.attn.num_head):
            q = np.full(attn.sequence_length * self.d, 0.0625).astype(np.float16).reshape(attn.sequence_length, self.d)
            kt= np.full(attn.sequence_length * self.d, 0.0625).astype(np.float16).reshape(self.d, attn.sequence_length)
            v = np.full(attn.sequence_length * self.d, 0.0625).astype(np.float16).reshape(attn.sequence_length, self.d)
            self.random_sparsity(q ,0.5)
            self.random_sparsity(kt,0.5)
            self.random_sparsity(v ,0.5)
            o = np.zeros(attn.sequence_length * self.d).astype(np.float16).reshape(attn.sequence_length, self.d)
            o_golden = np.zeros(attn.sequence_length * self.d).astype(np.float16).reshape(attn.sequence_length, self.d)
            self.compute_attention(q, kt, v, o_golden, attn.sequence_length, self.d)
            self.Q.append(q)
            self.KT.append(kt)
            self.V.append(v)
            self.O.append(o)
            self.O_golden.append(o_golden)
            self.map_heads_to_hbm_node(q,  self.Q_node_container,  self.Br_s, x, is_transposited=False, is_south=(self.attn.flatten_layot_method.index("Q") >= 2))
            self.map_heads_to_hbm_node(kt, self.KT_node_container, self.Bc_s, x, is_transposited=True,  is_south=(self.attn.flatten_layot_method.index("K") >= 2))
            self.map_heads_to_hbm_node(v,  self.V_node_container,  self.Bc_s, x, is_transposited=False, is_south=(self.attn.flatten_layot_method.index("V") >= 2))
            self.map_heads_to_hbm_node(o,  self.O_node_container,  self.Br_s, x, is_transposited=False, is_south=(self.attn.flatten_layot_method.index("O") >= 2))
            self.map_heads_to_hbm_node(o_golden,  self.O_golden_node_container,  self.Br_s, x, is_transposited=False, is_south=(self.attn.flatten_layot_method.index("O") >= 2))
            progress.update(1)
            pass
        progress.close()

    def compute_attention(self, q, kt, v, o, sequence_length, d):
        # Step 1: Compute QKᵀ
        scores = np.matmul(q, kt)
        # print("Compute QKᵀ")
        # print(scores)

        # Step 2: Scale scores by sqrt(d)
        scale_factor = np.sqrt(d)
        scaled_scores = scores / scale_factor
        # print("Scale scores by sqrt(d)")
        # print(scaled_scores)

        # Step 3: Apply softmax to scaled scores
        softmax_scores = np.exp(scaled_scores - np.max(scaled_scores, axis=-1, keepdims=True))  # Numerical stability
        softmax_scores /= softmax_scores.sum(axis=-1, keepdims=True)
        # print("Apply softmax to scaled scores")
        # print(softmax_scores)

        # Step 4: Multiply by V
        attention_output = np.matmul(softmax_scores, v)
        # print("Multiply by V")
        # print(attention_output)

        # Copy the result back to the provided output array `o`
        np.copyto(o, attention_output)

    def random_sparsity(self, matrix, sparsity):
        mask = np.random.rand(*matrix.shape) < sparsity
        matrix[mask] = 0.0
        pass

    def map_heads_to_hbm_node(self, matrix, container, slice_size, no_head, is_transposited=False, is_south=False):
        in_group_id = no_head // self.num_head_per_group
        in_group_id_x = in_group_id % self.num_group_x
        in_group_id_y = in_group_id // self.num_group_x
        node_offset = (in_group_id_x * self.attn.flatten_scale) if is_south else (in_group_id_y * self.attn.flatten_scale)

        for i in range(self.attn.sequence_length // slice_size):
            _slice_start = i * slice_size
            _slice_end = (i+1) * slice_size
            if is_transposited:
                _slice = matrix[:, _slice_start : _slice_end]
            else:
                _slice = matrix[_slice_start : _slice_end ,:]
                pass
            _store_node = node_offset + (i % self.attn.flatten_scale)
            container[_store_node].append(_slice)
            pass
        pass

    def gen_preload_arrays(self, is_golden=False):
        west_arrays  = []
        south_arrays = []

        container_dict = {}
        container_dict[self.attn.flatten_layot_method.index("Q")] = self.Q_node_container
        container_dict[self.attn.flatten_layot_method.index("K")] = self.KT_node_container
        container_dict[self.attn.flatten_layot_method.index("V")] = self.V_node_container
        container_dict[self.attn.flatten_layot_method.index("O")] = self.O_golden_node_container if is_golden else self.O_node_container

        for node in range(self.arch.num_cluster_y):
            a_list = container_dict[0][node]
            b_list = container_dict[1][node]
            merge = a_list + b_list
            merge = [arr.flatten() for arr in merge]
            np_array = np.concatenate(merge)
            west_arrays.append(np_array)
            pass

        for node in range(self.arch.num_cluster_x):
            a_list = container_dict[2][node]
            b_list = container_dict[3][node]
            merge = a_list + b_list
            merge = [arr.flatten() for arr in merge]
            np_array = np.concatenate(merge)
            south_arrays.append(np_array)
            pass

        return west_arrays, south_arrays
        pass

    def check_results(self):
        for x in range(self.attn.batch_size * self.attn.num_head):
            are_equal = np.allclose(self.O[x], self.O_golden[x], rtol=1e-1)
            epsilon = 1e-8
            relative_difference = np.abs(self.O[x] - self.O_golden[x]) / (np.abs(self.O_golden[x]) + epsilon)
            print(f"[Checking] head {x} Arrays are approximately equal: {are_equal}, with relative diff = {np.mean(relative_difference)}")
            pass
        pass

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

def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(description="Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("elf_path", type=str, help="Path of preload elf file")
    parser.add_argument("golden_folder", type=str, help="Path of folder containingn golden numbers")
    parser.add_argument(
        "module_paths",
        metavar="module_path",
        type=str,
        nargs="+",
        help="Paths to Python modules to import (absolute or relative)."
    )

    args = parser.parse_args()

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

    #instanciate arch and attn configurations
    attn = FlatAttetion()
    arch = FlexClusterArch()

    #Tiling information
    d           = attn.head_dimemsion
    Br          = attn.flatten_shape
    Bc          = attn.flatten_shape
    Tr          = attn.sequence_length // Br
    Tc          = attn.sequence_length // Bc
    Br_s        = attn.flatten_shape   // attn.flatten_scale;
    Bc_s        = attn.flatten_shape   // attn.flatten_scale;
    Q_slice     = Br_s * d * attn.elem_size
    K_slice     = Bc_s * d * attn.elem_size
    V_slice     = Bc_s * d * attn.elem_size
    O_slice     = Br_s * d * attn.elem_size

    #Group Information
    num_group_x = arch.num_cluster_x // attn.flatten_scale
    num_group_y = arch.num_cluster_y // attn.flatten_scale
    num_head_per_group = (attn.batch_size * attn.num_head) // (num_group_x * num_group_y)

    #Sanity check of HBM capacity overflow
    capacity_required_per_HBM_node_on_west_edge  = (Q_slice * Tr + K_slice * Tc) * num_head_per_group * num_group_x
    capacity_required_per_HBM_node_on_south_edge = (V_slice * Tr + O_slice * Tc) * num_head_per_group * num_group_y
    assert (capacity_required_per_HBM_node_on_west_edge <= arch.hbm_node_addr_space), \
        f"[Error] West HBM Node Overflow, {arch.hbm_node_addr_space} available but {capacity_required_per_HBM_node_on_west_edge} required"
    assert (capacity_required_per_HBM_node_on_south_edge <= arch.hbm_node_addr_space), \
        f"[Error] South HBM Node Overflow, {arch.hbm_node_addr_space} available but {capacity_required_per_HBM_node_on_south_edge} required"
    print("[Sanity Check] Passed")

    #Golden Model
    heads = Heads(attn, arch)

    #Prepare Preload Array
    west_arrays, south_arrays = heads.gen_preload_arrays(is_golden=False)
    
    #Prepara Address Array
    west_node_addresses = []
    for node in range(arch.num_cluster_y):
        west_node_addresses.append(
            arch.hbm_start_base + 
            node * arch.hbm_node_addr_space)
        pass

    south_node_addresses = []
    for node in range(arch.num_cluster_x):
        south_node_addresses.append(
            arch.hbm_start_base + 
            node * arch.hbm_node_addr_space + 
            arch.hbm_node_addr_space * 2 * arch.num_cluster_y + 
            arch.hbm_node_addr_space * arch.num_cluster_x)
        pass
    pass

    #Generate Preload ELF
    preload_array   = west_arrays + south_arrays
    preload_address = west_node_addresses + south_node_addresses
    script_dir = os.path.dirname(os.path.abspath(__file__))
    for addr in preload_address:
        print(f"0x{addr:016x}")
        pass
    pld.make_preload_elf(args.elf_path, preload_array, preload_address)

    #Prepare Golden Map
    west_arrays, south_arrays = heads.gen_preload_arrays(is_golden=True)
    golden_array   = west_arrays + south_arrays
    golden_address = west_node_addresses + south_node_addresses
    golden_address = np.array(golden_address)

    for arr_id in range(len(golden_array)):
        arr = golden_array[arr_id]
        np.save(f'{args.golden_folder}/hbm_node_{arr_id}.npy', arr)
        pass
    np.save(f'{args.golden_folder}/hbm_addresses.npy', golden_address)

if __name__ == "__main__":
    main()