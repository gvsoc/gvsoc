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
import re
import shutil
import numpy as np
import argparse
import importlib.util
from tqdm import tqdm
import preload as pld
from rich import print

def parse_hbm_file_as_fp16(filename):
    """
    Parses a file containing HBM offset blocks and their associated FP16 values in hex format.
    Returns a dictionary mapping offset (str) to a NumPy array of float16 values.
    """
    hbm_dict = {}
    current_offset = None
    offset_pattern = re.compile(r"^HBM\s+offset\s*==\s*\.(0x[0-9A-Fa-f]+):")
    hex_value_pattern = re.compile(r"^\s*(0x[0-9A-Fa-f]+)\s*$")

    with open(filename, "r") as f:
        lines = f.readlines()

    for line in tqdm(lines, desc="Parsing Result File"):
        line = line.strip()

        # Check for new offset block
        offset_match = offset_pattern.match(line)
        if offset_match:
            current_offset = offset_match.group(1)
            hbm_dict[current_offset] = []
            continue

        # Check for hex value line
        hex_match = hex_value_pattern.match(line)
        if hex_match and current_offset:
            hex_str = hex_match.group(1)
            hex_int = int(hex_str, 16)
            hbm_dict[current_offset].append(hex_int)

    # Convert each list of integers to NumPy float16 arrays
    for offset in hbm_dict:
        int_array = np.array(hbm_dict[offset], dtype=np.uint16)
        fp16_array = int_array.view(np.float16)
        hbm_dict[offset] = fp16_array

    return hbm_dict

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

def check_results():
    # Set up argument parser
    parser = argparse.ArgumentParser(description="Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("result_path", type=str, help="Path of result file")
    parser.add_argument("golden_folder", type=str, help="Path of folder containing golden numbers")
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

    #Instanciate arch and attn configurations
    attn = FlatAttetion()
    arch = FlexClusterArch()

    #Parse result file
    result = parse_hbm_file_as_fp16(args.result_path)

    #Load golden 
    golden_arrays = []
    golden_address = np.load(f"{args.golden_folder}/hbm_addresses.npy")
    for node_id in range(arch.num_cluster_x + arch.num_cluster_y):
        golden_arrays.append(np.load(f"{args.golden_folder}/hbm_node_{node_id}.npy").flatten())
        pass

    #Checking
    for offset, array in result.items():
        offset = int(offset, 16)
        real_addr = arch.hbm_start_base + offset

        #1. Locate node
        node_id = 0
        for nid in range(len(golden_address)):
            _start = golden_address[nid]
            _end = golden_address[nid] + arch.hbm_node_addr_space
            if real_addr >= _start and real_addr < _end:
                node_id = nid
                break
                pass
            pass

        #2. Slice golden array
        slice_start = (real_addr - golden_address[node_id]) // attn.elem_size
        slice_end = slice_start + len(array)
        # print(f"Dealing with HBM offest: {offset}")
        # print(f"I find node id: {node_id}")
        # print(f"The node has {len(golden_arrays[node_id])} elements")
        # print(f"I want to slice from {slice_start} to {slice_end}")
        golden = golden_arrays[node_id][slice_start:slice_end]
        # print(golden)
        are_equal = np.allclose(array, golden, rtol=1e-1)
        if are_equal:
            print(f"[green][{are_equal}][/green] Check at HBM offest [{offset:#x} : {(offset+attn.elem_size*len(array)):#x}]")
        else:
            print(f"[red][{are_equal}][/red] Check at HBM offest [{offset:#x} : {(offset+attn.elem_size*len(array)):#x}]")
        pass
    pass

if __name__ == "__main__":
    check_results()
