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
import io
import sys
import math
import shutil
import argparse
import numpy as np
import importlib.util
from tqdm import tqdm
import preload as pld

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

def gen_attention_preload_numpy_arrays(b, h, d, sq, skv, dt):
    dtype = np.uint8 if dt == "fp8" else np.uint16

    # Activation
    Q_np = np.ones((b, h, sq,  d), dtype=dtype)
    K_np = np.ones((b, h, skv, d), dtype=dtype)
    V_np = np.ones((b, h, skv, d), dtype=dtype)
    O_np = np.ones((b, h, sq,  d), dtype=dtype)

    O_empty = np.zeros(O_np.shape, dtype=O_np.dtype)
    O_golden = O_np

    return Q_np, K_np, V_np, O_empty, O_golden


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate C header files from a ATTN preload file and Dynamically import Python modules from absolute or relative paths.")
    parser.add_argument("elf_path", type=str, help="Path of preload elf file")
    parser.add_argument("header_path", type=str, help="Path to ATTN preload C header file")
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

    #basic parameters
    dtype               = attn.dtype
    kv_sequence_length  = attn.kv_sequence_length
    q_sequence_length   = attn.q_sequence_length * attn.speculative_length * int(attn.num_head // attn.num_head_group)
    head_dimemsion      = attn.head_dimemsion
    effective_head      = attn.num_head_group
    batch_size          = attn.batch_size

    #generate numpy array
    Q_np, K_np, V_np, O_empty, O_golden = gen_attention_preload_numpy_arrays(batch_size, effective_head, head_dimemsion, q_sequence_length, kv_sequence_length, dtype)
    Q_addr = arch.hbm_start_base
    O_eaddr = Q_addr + Q_np.nbytes
    O_gaddr = O_eaddr + O_empty.nbytes
    K_addr = arch.hbm_start_base + arch.hbm_node_addr_space * 2 * arch.num_cluster_y + arch.hbm_node_addr_space * arch.num_cluster_x
    V_addr = K_addr + K_np.nbytes
    print(f"Q_addr = {Q_addr: #x}")
    print(f"O_eaddr = {O_eaddr: #x}")
    print(f"O_gaddr = {O_gaddr: #x}")
    print(f"K_addr = {K_addr: #x}")
    print(f"V_addr = {V_addr: #x}")

    dumnp = np.array([1,1,1,1,1])
    pld.make_preload_elf(args.elf_path, 
        [dumnp,   dumnp,  dumnp,  dumnp,   dumnp],
        [Q_addr,  K_addr, V_addr, O_eaddr, O_gaddr])

    #generate preload header file
    with open(args.header_path, 'w') as file:
        file.write('#ifndef _ATTN_PRELOAD_H_\n')
        file.write('#define _ATTN_PRELOAD_H_\n\n')
        file.write(f'#define {"Q_addr".upper()} ((uint64_t){Q_addr: #x})\n')
        file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
        file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
        file.write(f'#define {"K_addr".upper()} ((uint64_t){K_addr: #x})\n')
        file.write(f'#define {"V_addr".upper()} ((uint64_t){V_addr: #x})\n')
        file.write('\n#endif // _ATTN_PRELOAD_H_\n')

    print(f'Header file "{args.header_path}" generated successfully.')

