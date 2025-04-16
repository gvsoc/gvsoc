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
from rich import print


def is_power_of_two(name, value):
    if value <= 0 or (value & (value - 1)) != 0:
        raise AssertionError(f"The value of '{name}' must be a power of two, but got {value}.")
    return True

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

def assertion():
    # Set up argument parser
    parser = argparse.ArgumentParser(description="Dynamically import Python modules from absolute or relative paths.")
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

    #Check Architecture Configurations
    is_power_of_two("arch.num_cluster_x", arch.num_cluster_x)
    is_power_of_two("arch.num_cluster_y", arch.num_cluster_y)
    assert (arch.num_cluster_x == arch.num_cluster_y), \
        f"[Arch Error] Current implementation we only support square shape cluster mesh"
    assert (arch.num_core_per_cluster >= 2), \
        f"[Arch Error] You must have at least 2 cores per cluster"
    assert (arch.spatz_attaced_core_list[0] == 0), \
        f"[Arch Error] The first core must attaching with a spatz"
    is_power_of_two("arch.spatz_num_vlsu_port", arch.spatz_num_vlsu_port)
    is_power_of_two("arch.spatz_num_function_unit", arch.spatz_num_function_unit)
    assert (arch.redmule_ce_height <= arch.redmule_ce_width * (arch.redmule_ce_pipe + 1)), \
        f"[Arch Error] Invalid RedMule setting, you need to follow this rule: arch.redmule_ce_height <= arch.redmule_ce_width * (arch.redmule_ce_pipe + 1)"
    is_power_of_two("arch.hbm_chan_placement[0]", arch.hbm_chan_placement[0])
    is_power_of_two("arch.hbm_chan_placement[3]", arch.hbm_chan_placement[3])
    is_power_of_two("arch.num_node_per_ctrl", arch.num_node_per_ctrl)
    assert (arch.hbm_chan_placement[0] >= (arch.num_cluster_y // arch.num_node_per_ctrl)), \
        f"[Arch Error] Invalid HBM channle configuration on west"
    assert (arch.hbm_chan_placement[3] >= (arch.num_cluster_x // arch.num_node_per_ctrl)), \
        f"[Arch Error] Invalid HBM channle configuration on west"

    #Check Attention Configuration
    is_power_of_two("attn.sequence_length", attn.sequence_length)
    is_power_of_two("attn.head_dimemsion", attn.head_dimemsion)
    is_power_of_two("attn.num_head", attn.num_head)
    is_power_of_two("attn.batch_size", attn.batch_size)
    is_power_of_two("attn.flatten_scale", attn.flatten_scale)
    is_power_of_two("attn.flatten_shape", attn.flatten_shape)
    assert (attn.elem_size == 2), \
        f"[Attn Error] Attention elem_size not support for {attn.elem_size}"
    assert (len(attn.flatten_layot_method) == 4 and \
            "Q" in attn.flatten_layot_method and \
            "K" in attn.flatten_layot_method and \
            "O" in attn.flatten_layot_method and \
            "V" in attn.flatten_layot_method), \
        f"[Attn Error] Invalide attn.flatten_layot_method: {attn.flatten_layot_method}"

    #Check Arch and Attn together
    assert (attn.flatten_scale <= arch.num_cluster_x), \
        f"[Config Error] Flatten Scale {attn.flatten_scale} should <= cluster mesh dimension {arch.num_cluster_x}"
    assert (attn.flatten_shape <= attn.sequence_length), \
        f"[Config Error] Flatten Shape {attn.flatten_shape} should <= MHA sequence length {attn.sequence_length}"
    assert (attn.flatten_scale < attn.flatten_shape), \
        f"[Config Error] Flatten Scale {attn.flatten_scale} should < Flatten Shape {attn.flatten_shape}"
    num_groups = (arch.num_cluster_x // attn.flatten_scale) * (arch.num_cluster_y // attn.flatten_scale)
    assert (attn.num_head * attn.batch_size >= num_groups), f"[Config Error] not enough head to feed {num_groups} groups for Naive FlatAttention"
    if attn.flatten_async:
        assert (attn.num_head * attn.batch_size >= 2 * num_groups), f"[Config Error] not enough head to feed {num_groups} groups for Async FlatAttention"
        pass
    d = attn.head_dimemsion
    s = attn.flatten_shape // attn.flatten_scale
    e = attn.elem_size
    if attn.flatten_async:
        L1_occupy = 2*(4*s*d*e + s*s*e + 5*s*e)
        assert (arch.cluster_tcdm_size >= L1_occupy), f"[Config Error] L1 area overflow: need {L1_occupy} bytes but L1 is {arch.cluster_tcdm_size} bytes"
    else:
        L1_occupy = 4*s*d*e + s*s*e + 5*s*e
        assert (arch.cluster_tcdm_size >= L1_occupy), f"[Config Error] L1 area overflow: need {L1_occupy} bytes but L1 is {arch.cluster_tcdm_size} bytes"
        pass
    MHA_HBM_area = 4 * attn.sequence_length * attn.head_dimemsion * attn.elem_size * attn.num_head * attn.batch_size
    assert (MHA_HBM_area <= (arch.num_cluster_x + arch.num_cluster_y) * arch.hbm_node_addr_space), \
        f"[Config Error] HBM area overflow, need {MHA_HBM_area} bytes in HBM but the arch is configured with {arch.hbm_node_addr_space} bytes node space in ({arch.num_cluster_x}x{arch.num_cluster_y}) cluster mesh"
    if attn.flatten_numerical_check:
        assert (MHA_HBM_area <= 0x40000000), f"[Config Error] We do not suggest to preload HBM over 1GB"
        pass

    print(f"[green]Configuration Assertion Passed ;)[/green]")

    pass

if __name__ == '__main__':
    assertion()