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
#         Kai Zhu   <kai.zhu@epfl.ch>

import os
import argparse
import importlib.util
import json
import math
import subprocess
import re

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
WORK_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
SOFTHIER_DIR = os.path.join(WORK_DIR, "SoftHier")
DEFAULT_ARCH_FILE = os.path.join(SOFTHIER_DIR, "soft_hier", "flex_cluster", "flex_cluster_arch.py")
DEFAULT_GEO_FILE = os.path.join(SOFTHIER_DIR, "geo.json")
DEFAULT_OUTPUT_DIR = SOFTHIER_DIR

## Change the roi accroding to the needed granularity.
def gen_roi(arch):
    roi_list = []
    for i in range(arch.num_cluster_x * arch.num_cluster_y):
        roi_list.append(f"chip/cluster_{i}/redmule")
        roi_list.append(f"chip/cluster_{i}/others")
        roi_list.append(f"chip/cluster_{i}/tcdm")
    return roi_list

def ice_template(name, position, dimension):
    return f"""{name} : 

    position {position[0]}, {position[1]}; 
    dimension {dimension[0]}, {dimension[1]}; 
    discretization  {round(dimension[0] / 10)}, {round(dimension[1] / 10)} ; 
"""

def roi2ice(geometry, roi, output_file):
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, "w") as f:
        for entry in roi:
            entry_name = entry.replace("/", "__")
            path=entry.split("/")
            geo=geometry.copy()
            position = [0, 0]
            for i in range(len(path)):
                if i == len(path) - 1:
                    dimension = geo[path[i]]["shape"]
                    position[0] += geo[path[i]]["offset"][0]
                    position[1] += geo[path[i]]["offset"][1]
                    ice_entry = ice_template(entry_name, position, dimension)
                    f.write(ice_entry + "\n")
                else:
                    position[0] += geo[path[i]]["offset"][0]
                    position[1] += geo[path[i]]["offset"][1]
                    geo = geo[path[i]]["subs"]
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

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate ROI for ICE")
    parser.add_argument("arch_file",  nargs="?", default=DEFAULT_ARCH_FILE, help="Path to architecture configuration python file")
    parser.add_argument("geo_file",  nargs="?", default=DEFAULT_GEO_FILE, help="Path to geometry configuration python file")
    parser.add_argument("output_dir", nargs="?", default=DEFAULT_OUTPUT_DIR, help="Output directory for floorplan_nopower.flp")
    args = parser.parse_args()
    arch_file = args.arch_file
    geo_file = args.geo_file
    output_dir = args.output_dir

    # Process each provided module path
    for module_path in [arch_file]:
        # Convert to absolute path
        absolute_path = os.path.abspath(module_path)

        if not os.path.isfile(absolute_path):
            print(f"Error: {absolute_path} is not a valid file.")
            continue

        try:
            # Import the module dynamically
            module = import_module_from_path(absolute_path)
            # print(f"Successfully imported: {module.__name__} from {absolute_path}")
        except Exception as e:
            print(f"Failed to import {absolute_path}: {e}")

    arch = FlexClusterArch()
    with open(geo_file, "r") as f:
        geometry = json.load(f)

    # Generate ROI
    roi = gen_roi(arch)

    # Convert ROI to ICE format
    output_file = os.path.join(output_dir, "floorplan_nopower.flp")
    roi2ice(geometry, roi, output_file)