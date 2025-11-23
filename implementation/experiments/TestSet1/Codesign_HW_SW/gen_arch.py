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
from tqdm import tqdm
from rich import print
from pathlib import Path
from tabulate import tabulate

class FlexClusterArch:

    def __init__(self):

        #Cluster
        self.num_cluster_x           = 32
        self.num_cluster_y           = 32
        self.num_core_per_cluster    = 5

        self.cluster_tcdm_bank_width = 32
        self.cluster_tcdm_bank_nb    = 128

        self.cluster_tcdm_base       = 0x00000000
        self.cluster_tcdm_size       = 0x00060000
        self.cluster_tcdm_remote     = 0x30000000

        self.cluster_stack_base      = 0x10000000
        self.cluster_stack_size      = 0x00020000

        self.cluster_zomem_base      = 0x18000000
        self.cluster_zomem_size      = 0x00020000

        self.cluster_reg_base        = 0x20000000
        self.cluster_reg_size        = 0x00000200

        #Spatz Vector Unit
        self.spatz_attaced_core_list = [0, 1, 2, 3]
        self.spatz_num_vlsu_port     = 8
        self.spatz_num_function_unit = 4

        #RedMule
        self.redmule_ce_height       = 32
        self.redmule_ce_width        = 16
        self.redmule_ce_pipe         = 1
        self.redmule_elem_size       = 2
        self.redmule_queue_depth     = 1
        self.redmule_reg_base        = 0x20020000
        self.redmule_reg_size        = 0x00000200

        #IDMA
        self.idma_outstand_txn       = 64
        self.idma_outstand_burst     = 256

        #HBM
        self.hbm_start_base          = 0xc0000000
        self.hbm_node_addr_space     = 0xc0000000
        self.num_node_per_ctrl       = 32
        self.hbm_chan_placement      = [0,0,0,64]
        self.hbm_node_aliase         = 32
        self.hbm_ctrl_xor_scrambling = 1

        #NoC
        self.noc_outstanding         = 64
        self.noc_link_width          = 1024

        #System
        self.instruction_mem_base    = 0x80000000
        self.instruction_mem_size    = 0x00010000

        self.soc_register_base       = 0x90000000
        self.soc_register_size       = 0x00010000
        self.soc_register_eoc        = 0x90000000
        self.soc_register_wakeup     = 0x90000004

        #Synchronization
        self.sync_base               = 0x40000000
        self.sync_interleave         = 0x00000080
        self.sync_special_mem        = 0x00000040

        #Chip Frequence
        self.frequence               = 965000000


def write_arch_file(obj, filename):
    cls_name = obj.__class__.__name__
    attrs = vars(obj)  # gets all instance attributes

    with open(filename, 'w') as f:
        f.write(f"class {cls_name}:\n")
        f.write("    def __init__(self):\n")
        if not attrs:
            f.write("        pass\n")
        else:
            for key, value in attrs.items():
                if 'sync_' in key or 'soc_register' in key or 'base' in key or 'size' in key or 'addr_space' in key or 'remote' in key:
                    f.write(f"        self.{key} = {value:#010x}\n")
                else:
                    f.write(f"        self.{key} = {repr(value)}\n")
    pass

def gen_arch():
    # Defualt path
    current_dir = Path(__file__).resolve().parent
    defualt_dir = current_dir / "arch_candidates"
    print(defualt_dir)

    # Create Folder
    os.makedirs(defualt_dir, exist_ok=True)

    # Varaibles
    mesh_dim_list = [64, 32, 16, 8]
    spat_num_list = [1, 2, 4]

    # Generate candidates
    for d in mesh_dim_list:
        for s in spat_num_list:
            filename = defualt_dir / f"arch{d}x{d}_spatz{s}.py"
            scale_f = (32 / d)
            spatz_f = (4 // s)
            arch = FlexClusterArch()
            arch.num_cluster_x              = d
            arch.num_cluster_y              = d
            arch.num_core_per_cluster       = s + 1
            arch.cluster_tcdm_bank_nb       = int(128 * scale_f)
            arch.cluster_tcdm_size          = int(0x00060000 * scale_f * scale_f) if d <= 32 else int(0x00060000 * scale_f)
            arch.cluster_stack_size         = int(0x00020000 * scale_f * scale_f) if d <= 32 else int(0x00020000 * scale_f)
            arch.cluster_zomem_size         = int(0x00020000 * scale_f * scale_f) if d <= 32 else int(0x00020000 * scale_f)
            arch.spatz_attaced_core_list    = list(range(0, s))
            arch.spatz_num_vlsu_port        = int(8 * scale_f * scale_f * spatz_f)
            arch.spatz_num_function_unit    = int(4 * scale_f * scale_f * spatz_f)
            arch.redmule_ce_height          = int(32 * scale_f)
            arch.redmule_ce_width           = int(16 * scale_f)
            arch.num_node_per_ctrl          = d
            arch.hbm_chan_placement         = [0, 0, 0, 2 * d]
            arch.hbm_node_aliase            = d
            arch.noc_link_width             = int(1024 * scale_f)
            write_arch_file(arch, filename)
            pass


    pass


if __name__ == '__main__':
    gen_arch()
