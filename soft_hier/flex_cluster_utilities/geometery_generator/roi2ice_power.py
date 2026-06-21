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
import argparse
import importlib.util
import json
import math
import subprocess
import re

## Change the roi accroding to the needed granularity.
def gen_roi(arch):
    roi_list = []
    for i in range(arch.num_cluster_x * arch.num_cluster_y):
        roi_list.append(f"chip/cluster_{i}/redmule")
        roi_list.append(f"chip/cluster_{i}/tcdm")
    return roi_list

def ice_template(name, position, dimension, power_values):
    return f"""{name} : 

    position {position[0]}, {position[1]}; 
    dimension {dimension[0]}, {dimension[1]}; 
    power values {', '.join(map(str, power_values))};
"""

def roi2ice(power_file, geometry, roi):
    power_line = ""
    for entry in roi:
        # entry_name = re.sub(r'[^\w.\-]', '_', entry) # Replace non-alphanumeric characters with underscores
        entry_name = entry
        result = subprocess.run(
            ["grep", entry, power_file],
            capture_output=True,
            text=True
        )
        power_value = float(result.stdout.split("\n")[0].split(";")[3])
        power_line += f"{power_value} "
        pass
    print(power_line)
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
    parser.add_argument("arch_file",  nargs="?", help="Path to architecture configuration python file")
    parser.add_argument("geo_file",  nargs="?", help="Path to geometry configuration python file")
    parser.add_argument("power_file",  nargs="?", help="Path to power configuration python file")
    args = parser.parse_args()
    arch_file = args.arch_file
    geo_file = args.geo_file
    power_file = args.power_file

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
    roi2ice(power_file, geometry, roi)
