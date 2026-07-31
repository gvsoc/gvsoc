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
gemm = SummaGEMM()

# Dealing with Input/Output matrix reshaping
appendix = []
if gemm.resha_x_from_enable:
    resha_x_m_size = gemm.resha_x_from_m
    resha_x_k_size = (gemm.m_size * gemm.k_size) // gemm.resha_x_from_m
    assert gemm.m_size % gemm.m_tile == 0, f"X Reshaped Enabled, We must make sure M size and tile are aligned"
    assert gemm.k_size % gemm.k_tile == 0, f"X Reshaped Enabled, We must make sure K size and tile are aligned"
    assert resha_x_k_size > 0, f"Invalide X Reshape {resha_x_m_size}, {resha_x_k_size}"
    assert resha_x_m_size >= gemm.m_tile, f"X Reshape Cross M Tiles"
    assert resha_x_k_size >= gemm.k_tile, f"X Reshape Cross K Tiles"
    assert resha_x_m_size % gemm.m_tile == 0, f"X Reshaped Enabled, We must make sure reshaped M size and tile are aligned"
    assert resha_x_k_size % gemm.k_tile == 0, f"X Reshaped Enabled, We must make sure reshaped K size and tile are aligned"
    if gemm.resha_x_from_m > gemm.m_size:
        # Tall
        appendix.append(f"#define GEMM_RESHAPE_X_FROM_K ((uint64_t){resha_x_k_size})")
        appendix.append(f"#define GEMM_RESHA_X_FROM_TALL")
    elif gemm.resha_x_from_m < gemm.m_size:
        # Thin
        appendix.append(f"#define GEMM_RESHAPE_X_FROM_K ((uint64_t){resha_x_k_size})")
        appendix.append(f"#define GEMM_RESHA_X_FROM_THIN")
    else:
        raise RuntimeError(f"X Reshaped Enabled But Dimension Not Changed")
        pass
    pass

if gemm.resha_z_to_enable:
    resha_z_m_size = gemm.resha_z_to_m
    resha_z_n_size = (gemm.m_size * gemm.n_size) // gemm.resha_z_to_m
    assert gemm.m_size % gemm.m_tile == 0, f"Z Reshaped Enabled, We must make sure M size and tile are aligned"
    assert gemm.n_size % gemm.n_tile == 0, f"Z Reshaped Enabled, We must make sure N size and tile are aligned"
    assert resha_z_n_size > 0, f"Invalide Z Reshape {resha_z_m_size}, {resha_z_n_size}"
    assert resha_z_m_size >= gemm.m_tile, f"Z Reshape Cross M Tiles"
    assert resha_z_n_size >= gemm.n_tile, f"Z Reshape Cross N Tiles"
    assert resha_z_m_size % gemm.m_tile == 0, f"Z Reshaped Enabled, We must make sure reshaped M size and tile are aligned"
    assert resha_z_n_size % gemm.n_tile == 0, f"Z Reshaped Enabled, We must make sure reshaped N size and tile are aligned"
    if gemm.resha_z_to_m > gemm.m_size:
        # Tall
        appendix.append(f"#define GEMM_RESHAPE_Z_TO_N ((uint64_t){resha_z_n_size})")
        appendix.append(f"#define GEMM_RESHA_Z_TO_TALL")
    elif gemm.resha_z_to_m < gemm.m_size:
        # Thin
        appendix.append(f"#define GEMM_RESHAPE_Z_TO_N ((uint64_t){resha_z_n_size})")
        appendix.append(f"#define GEMM_RESHA_Z_TO_THIN")
    else:
        raise RuntimeError(f"Z Reshaped Enabled But Dimension Not Changed")
        pass
    pass
kc.generate_config_C_header("GEMM", gemm, C_header_file, gemm.dtype, gemm.summa_numer, appendix=appendix)
