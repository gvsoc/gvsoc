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
import utils.numerical_check_utils as num_util

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

def moed_check_results():
    # Set up argument parser
    parser = argparse.ArgumentParser(description="Check numerical correctness for MoEDispatch.")
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

    #Instanciate arch and moed configurations
    moed = MoEDispatch()

    #Set up data type
    dtype = torch.int32

    #Parse result file
    result = num_util.parse_hbm_file(args.result_path, dtype)

     #Seperate idx and pos
    idx_tensor_list = []
    pos_tensor_list = []
    idx_offset_list = []
    pos_offset_list = []
    i = 0
    for offset, array in result.items():
        if i % 2 == 0:
            #idx
            idx_tensor_list.append(array)
            idx_offset_list.append(offset)
        else:
            #pos
            pos_tensor_list.append(array)
            pos_offset_list.append(offset)
            pass
        i = i + 1
        pass
    if len(idx_tensor_list) > len(pos_tensor_list):
        idx_tensor_list.pop()
        pass
    idx_tensor = torch.cat(idx_tensor_list).view(moed.num_tokens, moed.num_active_experts)
    pos_tensor = torch.cat(pos_tensor_list).view(moed.num_tokens, moed.num_active_experts)

    #Check idx & pos self-consistency
    scoreboard = []
    for x in range(moed.num_routed_experts):
        scoreboard.append([])
        pass
    for i in tqdm(range(moed.num_tokens), desc="[Create Scoreboard]"):
        for j in range(moed.num_active_experts):
            scoreboard[idx_tensor[i,j].item()].append(pos_tensor[i,j].item())
            pass
        pass
    for x in tqdm(range(moed.num_routed_experts), desc="[Check Self-Consistency]"):
        lst = scoreboard[x]
        assert len(lst) == len(set(lst)), f"[Error] {x} th expert has duplicate positon: {lst}"
        nums_sorted = sorted(lst)
        assert nums_sorted == list(range(min(lst), max(lst) + 1)), f"[Error] {x} th expert has non-continuous positon: {nums_sorted}"
        pass
    print(f"[green][{True}] Perfect! MoE Dispatch is Self-Consistent[/green]")

    pass

if __name__ == "__main__":
    moed_check_results()
