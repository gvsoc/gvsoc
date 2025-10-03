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
import numpy as np
import importlib.util
from tqdm import tqdm
import preload as pld
from rich import print
import torch.nn.functional as F

def parse_hbm_file(filename, dtype):
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

    # Convert each list of integers to Torch<dtype> arrays
    for offset in hbm_dict:
        int_array = torch.tensor(hbm_dict[offset], dtype=torch.uint16)
        dtype_array = int_array.view(dtype)
        hbm_dict[offset] = dtype_array

    return hbm_dict

def get_metric(A, B):
    name = "Mean Squared Error"
    mse = torch.mean((A - B)**2)
    return name, mse

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
    parser = argparse.ArgumentParser(description="Check numerical correctness for attention.")
    parser.add_argument("result_path", type=str, help="Path of result file")
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
    dtype = torch.float8_e5m2 if attn.dtype == 'fp8' else torch.float16

    #Parse result file
    result = parse_hbm_file(args.result_path, dtype)

    #Seperate output and golden
    output_tensor_list = []
    golden_tensor_list = []
    i = 0
    for offset, array in result.items():
        if i % 2 == 0:
            #output
            output_tensor_list.append(array)
        else:
            #golden
            golden_tensor_list.append(array)
            pass
        i = i + 1
        pass
    if len(output_tensor_list) > len(golden_tensor_list):
        output_tensor_list.pop()
        pass
    output_tensor = torch.cat(output_tensor_list)
    golden_tensor = torch.cat(golden_tensor_list)

    #Check results
    metric_name, metric = get_metric(output_tensor.to(torch.float32), golden_tensor.to(torch.float32))
    nbytes = output_tensor.numel() * output_tensor.element_size()
    are_equal = False
    if metric <= 0.05:
        are_equal = True
        print(f"[green][{are_equal}][/green] Check output size of {nbytes: #x} : [{metric_name}] = {metric}")
    else:
        are_equal = False
        print(f"[red][{are_equal}][/red] Check output size of {nbytes: #x} : [{metric_name}] = {metric}")

    #List Chunk Information if numerical not pass
    if are_equal == False:
        print(f"[yellow]Detailed Results:[/yellow]")
        for x in range(len(output_tensor_list)):
            output_tensor = output_tensor_list[x]
            golden_tensor = golden_tensor_list[x]
            metric_name, metric = get_metric(output_tensor.to(torch.float32), golden_tensor.to(torch.float32))
            nbytes = output_tensor.numel() * output_tensor.element_size()
            are_equal = False
            if metric <= 0.05:
                are_equal = True
                print(f"    [green][{are_equal}][/green] Check chunk {x} with size of {nbytes: #x} : [{metric_name}] = {metric}")
            else:
                are_equal = False
                print(f"    [red][{are_equal}][/red] Check chunk {x} with size of {nbytes: #x} : [{metric_name}] = {metric}")
            pass
        pass
    pass

if __name__ == "__main__":
    check_results()
