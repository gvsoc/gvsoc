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

parser = argparse.ArgumentParser(description="Generate C header files from a ATTN configuration file.")
parser.add_argument("input_file",  nargs="?", help="Path to ATTN configuration python file")
parser.add_argument("output_file", nargs="?", help="Path to ATTN configuration C header file")
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
    file.write('#ifndef _ATTN_CONFIG_H_\n')
    file.write('#define _ATTN_CONFIG_H_\n\n')
    flatten_numer = '0'
    dtype = 'fp16'
    
    for attr_name, attr_value in attributes.items():
        # Convert attribute name to uppercase and prefix with 'ARCH_'
        define_name = f'ATTN_{attr_name.upper()}'
        if define_name == 'ATTN_DTYPE':
            dtype = attr_value
            continue
        if define_name == 'ATTN_FLATTEN_NUMER':
            flatten_numer = attr_value
        file.write(f'#define {define_name} ((uint64_t){attr_value})\n')

    if dtype == "'fp16'":
        file.write(f'#define ATTN_FP16\n')
        file.write(f'#define DATA_TYPE_WIDTH             16\n')
        file.write(f'#define DATA_TYPE_BYTE              2\n')
        file.write(f'typedef uint16_t                    attn_data_t;\n')
        if flatten_numer == '1':
            file.write(f'#define REDMULE_COMPUTE_TYPE        REDMULE_FP_16\n')
            file.write(f'#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_FP_16\n')
            file.write(f'#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_FP_16\n')
        else:
            file.write(f'#define REDMULE_COMPUTE_TYPE        REDMULE_NONE_16\n')
            file.write(f'#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_NONE\n')
            file.write(f'#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_NONE\n')
    elif dtype == "'fp8'":
        file.write(f'#define ATTN_FP8\n')
        file.write(f'#define DATA_TYPE_WIDTH             8\n')
        file.write(f'#define DATA_TYPE_BYTE              1\n')
        file.write(f'typedef uint8_t                     attn_data_t;\n')
        if flatten_numer == '1':
            file.write(f'#define REDMULE_COMPUTE_TYPE        REDMULE_FP_8\n')
            file.write(f'#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_FP_8\n')
            file.write(f'#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_FP_8\n')
        else:
            file.write(f'#define REDMULE_COMPUTE_TYPE        REDMULE_UINT_8\n')
            file.write(f'#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_NONE\n')
            file.write(f'#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_NONE\n')
    
    file.write('#define STR(x) #x\n')
    file.write('#define XSTR(x) STR(x)\n')
    file.write('\n#endif // _ATTN_CONFIG_H_\n')

print(f'Header file "{C_header_file}" generated successfully.')

