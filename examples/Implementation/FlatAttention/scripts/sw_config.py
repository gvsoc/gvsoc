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
import argparse

parser = argparse.ArgumentParser(description="Generate C header files from a Flatten Attention configuration file.")
parser.add_argument("input_file",  nargs="?", help="Path to Flatten Attention configuration python file")
parser.add_argument("output_file", nargs="?", help="Path to Flatten Attention configuration C header file")
args = parser.parse_args()
input_file = args.input_file

# Read the input Python file
C_header_file = args.output_file

# Initialize a dictionary to store the class attributes and their values
attributes = {}

# Read the input file and extract the class attributes
with open(input_file, 'r') as file:
    lines = file.readlines()
    for line in lines:
        match = re.match(r'\s*self\.(\w+)\s*=\s*(.+)', line)
        if match:
            attr_name = match.group(1)
            attr_value = match.group(2)
            attributes[attr_name] = attr_value

# Write the output C header file
with open(C_header_file, 'w') as file:
    file.write('#ifndef _FLAT_ATTENTION_CONFIG_H_\n')
    file.write('#define _FLAT_ATTENTION_CONFIG_H_\n\n')
    
    for attr_name, attr_value in attributes.items():
        # Convert attribute name to uppercase and prefix with 'ARCH_'
        define_name = f'ATTN_{attr_name.upper()}'
        if define_name == 'ATTN_FLATTEN_LAYOT_METHOD':
            define_name = 'ATTN_FLATTEN_LAYOT_TILED'
            if "QOKV" in attr_value:
                file.write(f'#define TILED_HBM_Q(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_O(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size + group_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_KT(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y) hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_V(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size + group_y * num_head_per_group * T * slice_size))\n')
                pass
            elif "QKOV" in attr_value:
                file.write(f'#define TILED_HBM_Q(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_KT(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y) hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size + group_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_O(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_V(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size + group_y * num_head_per_group * T * slice_size))\n')
                pass
            elif "QVOK" in attr_value:
                file.write(f'#define TILED_HBM_Q(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_V(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size + group_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_O(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_KT(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y) hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size + group_y * num_head_per_group * T * slice_size))\n')
                pass
            elif "OKQV" in attr_value:
                file.write(f'#define TILED_HBM_O(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_KT(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y) hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size + group_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_Q(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_V(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size + group_y * num_head_per_group * T * slice_size))\n')
                pass
            elif "OVQK" in attr_value:
                file.write(f'#define TILED_HBM_O(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_V(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size + group_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_Q(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_KT(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y) hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size + group_y * num_head_per_group * T * slice_size))\n')
                pass
            elif "KVQO" in attr_value:
                file.write(f'#define TILED_HBM_KT(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y) hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_V(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_west(pos.y,(group_id_x * num_head_per_group * T * slice_size + group_x * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_Q(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size))\n')
                file.write(f'#define TILED_HBM_O(pos,T,slice_size,num_head_per_group,group_id_x,group_id_y,group_x,group_y)  hbm_south(pos.x,(group_id_y * num_head_per_group * T * slice_size + group_y * num_head_per_group * T * slice_size))\n')
                pass
            pass
        file.write(f'#define {define_name} {attr_value}\n')
    
    file.write('\n#endif // _FLAT_ATTENTION_CONFIG_H_\n')

print(f'Header file "{C_header_file}" generated successfully.')

