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

import re
import os
import argparse
import importlib.util
import utils.kernel_configuration as kc

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

parser = argparse.ArgumentParser(description="Generate C header files from a GEMM configuration file.")
parser.add_argument("input_file",  nargs="?", help="Path to GEMM configuration python file")
parser.add_argument("output_file", nargs="?", help="Path to GEMM configuration C header file")
args = parser.parse_args()
input_file = args.input_file

# Read the input Python file
C_header_file = args.output_file

# Process each provided module path
for module_path in [input_file]:
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

# Generate the C header file
rope = RoPE()
kc.generate_config_C_header("ROPE", rope, C_header_file, rope.dtype, rope.rope_numer)
