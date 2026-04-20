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

'''
Entry Format:
{
    "name"      : str
    "type"      : str in ["chip", "die", "comp"]
    "shape"     : (,) unit um
    "offset"    : (,) unit um
    "subs"      : [] list of sub entries
}
'''

PnR_uti = 0.66
kGE_to_um2 = {
    "22nm" : 200,
    "12nm" : 120,
    "7nm"  :  60,
    "5nm"  :  30,
}

SRAM_bitcell_um2 = {
    "22nm" : 0.100,
    "12nm" : 0.060,
    "7nm"  : 0.027,
    "5nm"  : 0.021,
}

"""
FloorPlan Assumption

    -------------------------------------
    | 2.Core/spatz/iDMA |               |
    |    NoCRouter      |               |
    --------------------|               |
    |                   |               |
    |                   |               |
    |                   | 3. L1 Memory  |
    |    1. RedMule     |               |
    |       Square      |               |
    |                   |               |
    |                   |               |
    -------------------------------------
    
"""

def gen_clusters_subs(arch):
    subs = []
    tech_node = ("5nm" if not hasattr(arch, 'tech_node') else arch.tech_node)

    #1. RedMule
    redmule_per_ce_kGE = 8.59
    redmule_others_kGE = 100
    redmule_kGE = redmule_others_kGE + arch.redmule_ce_height * arch.redmule_ce_width * redmule_per_ce_kGE
    redmule_area = redmule_kGE * kGE_to_um2[tech_node] / PnR_uti
    redmule_dim = math.sqrt(redmule_area) #unit um
    redmule_entry = {
        "name"  : "redmule",
        "type"  : "comp",
        "shape" : (redmule_dim, redmule_dim),
        "offset": (0, 0),
        "subs"  : [],
    }
    subs.append(redmule_entry)

    #2. other components
    snitch_kGE = 25 + 126 #core + IPU
    spatz_kGE = 169 + 46 + arch.spatz_num_function_unit * 142 #VRF + VLSU + FPUs
    iDMA_kGE = 7 + arch.idma_outstand_txn * 6.5 + arch.noc_link_width * 1.3 / 32 #bias + O(Outstanding) + O(Datawidth)
    NoC_Router_kGE = 28 + 168 * arch.noc_link_width / 512 #NI + Router
    all_other_kGE = snitch_kGE * arch.num_core_per_cluster + spatz_kGE * len(arch.spatz_attaced_core_list) + iDMA_kGE + NoC_Router_kGE
    all_other_area = all_other_kGE * kGE_to_um2[tech_node] / PnR_uti
    all_other_width = redmule_dim
    all_other_height = all_other_area / all_other_width
    all_other_entry = {
        "name"  : "others",
        "type"  : "comp",
        "shape" : (all_other_width, all_other_height),
        "offset": (0, redmule_dim),
        "subs"  : [],
    }
    subs.append(all_other_entry)

    #3. L1 Memory
    L1_area = arch.cluster_tcdm_size * 8 * SRAM_bitcell_um2[tech_node]
    L1_height = all_other_height + redmule_dim
    L1_width = L1_area / L1_height
    L1_entry = {
        "name"  : "tcdm",
        "type"  : "comp",
        "shape" : (L1_width, L1_height),
        "offset": (redmule_dim, 0),
        "subs"  : [],
    }
    subs.append(L1_entry)

    cluster_shape = (redmule_dim + L1_width, redmule_dim + all_other_height)
    return subs, cluster_shape
    pass

def gen_die(arch):
    clusters = []

    subs, shape = gen_clusters_subs(arch)
    for y in range(arch.num_cluster_y):
        for x in range(arch.num_cluster_x):
            clusters.append({
                "name"  : f"cluster_{x + y * arch.num_cluster_x}",
                "type"  : "comp",
                "shape" : shape,
                "offset": (x * shape[0], y * shape[1]),
                "subs"  : subs,
            })
            pass
    
    die_shape = (arch.num_cluster_x * shape[0], arch.num_cluster_y * shape[1])
    return clusters, die_shape
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

parser = argparse.ArgumentParser(description="Generate Geometry Information for SoftHier")
parser.add_argument("arch_file",  nargs="?", help="Path to architecture configuration python file")
parser.add_argument("geo_file", nargs="?", help="Path to geometry information file")
args = parser.parse_args()
arch_file = args.arch_file

# Read the input Python file
geo_file = args.geo_file

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
        print(f"Successfully imported: {module.__name__} from {absolute_path}")
    except Exception as e:
        print(f"Failed to import {absolute_path}: {e}")

arch = FlexClusterArch()
die_subs, die_shape = gen_die(arch)
die_info = {
    "name"  : "SoftHier Compute Die",
    "type"  : "die",
    "shape" : die_shape,
    "offset": (0, 0),
    "subs"  : die_subs,
}

with open(geo_file, "w") as f:
    json.dump(die_info, f, indent=4)