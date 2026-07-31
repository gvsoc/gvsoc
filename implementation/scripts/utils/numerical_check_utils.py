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
import matplotlib.pyplot as plt

def overflow_check(tensor, reference, threshold=4096):
    """
    Replace elements in `tensor` whose absolute value exceeds `threshold`
    with the corresponding elements from `reference`.

    Args:
        tensor (torch.Tensor): Input tensor to check and modify.
        reference (torch.Tensor): Reference tensor (same shape and dtype as `tensor`).
        threshold (float): Absolute value threshold for replacement (default: 4096).

    Returns:
        modified_tensor (torch.Tensor): Tensor with large values replaced.
        num_replaced (int): Number of elements replaced.
    """
    # Ensure shapes and dtypes match
    if tensor.shape != reference.shape:
        raise ValueError("`tensor` and `reference` must have the same shape.")
    if tensor.dtype != reference.dtype:
        raise ValueError("`tensor` and `reference` must have the same dtype.")

    # Mask of values exceeding the threshold
    mask = tensor.abs() > threshold

    # Count how many elements were replaced
    num_replaced = mask.sum().item()

    # Replace those elements with values from the reference tensor
    modified_tensor = tensor.clone()
    modified_tensor[mask] = reference[mask]

    return modified_tensor, num_replaced

def plot_tensor_distribution(tensor, bins=50, title="Tensor Value Distribution", adaptive_range=True):
    """
    Plots the numerical distribution of a PyTorch tensor.

    Args:
        tensor (torch.Tensor): The tensor whose values to plot.
        bins (int): Number of histogram bins (default: 50).
        title (str): Title of the plot.
        adaptive_range (bool): If True, histogram range adapts to tensor's min/max.
    """
    # Move tensor to CPU and flatten
    data = tensor.detach().cpu().numpy().flatten()

    # Determine range dynamically
    hist_range = (data.min(), data.max()) if adaptive_range else None

    # Plot histogram
    plt.hist(data, bins=bins, range=hist_range, edgecolor='black')
    plt.title(title)
    plt.xlabel("Value")
    plt.ylabel("Frequency")
    plt.grid(True, linestyle='--', alpha=0.5)

    # Optional: Show min and max in the title
    if adaptive_range:
        plt.title(f"{title}\n(min={data.min():.4f}, max={data.max():.4f})")

    plt.show()

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

def check_results(result_path, dtype_str):
    #Set up data type
    dtype = torch.float8_e5m2 if dtype_str == 'fp8' else torch.float16

    #Parse result file
    result = parse_hbm_file(result_path, dtype)

     #Seperate output and golden
    output_tensor_list = []
    golden_tensor_list = []
    output_offset_list = []
    golden_offset_list = []
    i = 0
    for offset, array in result.items():
        if i % 2 == 0:
            #output
            output_tensor_list.append(array)
            output_offset_list.append(offset)
        else:
            #golden
            golden_tensor_list.append(array)
            golden_offset_list.append(offset)
            pass
        i = i + 1
        pass
    if len(output_tensor_list) > len(golden_tensor_list):
        output_tensor_list.pop()
        pass
    output_tensor = torch.cat(output_tensor_list)
    golden_tensor = torch.cat(golden_tensor_list)

    #Numerical overflow analysis
    output_tensor, num_overflow = overflow_check(output_tensor, golden_tensor)
    if num_overflow != 0:
        print(f"[yellow][WARN][/yellow] Overflowed Numbers = {num_overflow} out of {output_tensor.numel()} | Overflow Ratio = {num_overflow / output_tensor.numel()}")
    else:
        print(f"[green]Perfect! No Overflowed Numbers[/green]")
        pass

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
                print(f"    [green][{are_equal}][/green] Check chunk {x} start at {output_offset_list[x]} with size of {nbytes: #x} : [{metric_name}] = {metric}")
            else:
                are_equal = False
                print(f"    [red][{are_equal}][/red] Check chunk {x} start at {output_offset_list[x]} with size of {nbytes: #x} : [{metric_name}] = {metric}")
            pass
        pass
    pass

