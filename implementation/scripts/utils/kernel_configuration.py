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

def generate_config_C_header(header_prefix, config, C_header_file, dtype, numerical_check):

    with open(C_header_file, 'w') as file:
        file.write(f'#ifndef _{header_prefix.upper()}_CONFIG_H_\n')
        file.write(f'#define _{header_prefix.upper()}_CONFIG_H_\n\n')
        
        for attr_name, attr_value in vars(config).items():
            # Convert attribute name to uppercase and prefix with 'ARCH_'
            define_name = f'{header_prefix.upper()}_{attr_name.upper()}'
            if define_name == f'{header_prefix.upper()}_DTYPE':
                continue
            if isinstance(attr_value, str):
                file.write(f'#define {define_name}_{attr_value.upper()}\n')
                continue
            file.write(f'#define {define_name} ((uint64_t){attr_value})\n')

        if dtype == 'fp16':
            file.write(f'#define {header_prefix.upper()}_FP16\n')
            file.write(f'#define DATA_TYPE_WIDTH             16\n')
            file.write(f'#define DATA_TYPE_BYTE              2\n')
            file.write(f'typedef uint16_t                    {header_prefix.lower()}_data_t;\n')
            if numerical_check == 1:
                file.write(f'#define REDMULE_COMPUTE_TYPE        REDMULE_FP_16\n')
                file.write(f'#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_FP_16\n')
                file.write(f'#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_FP_16\n')
            else:
                file.write(f'#define REDMULE_COMPUTE_TYPE        REDMULE_NONE_16\n')
                file.write(f'#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_NONE\n')
                file.write(f'#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_NONE\n')
        elif dtype == 'fp8':
            file.write(f'#define {header_prefix.upper()}_FP8\n')
            file.write(f'#define DATA_TYPE_WIDTH             8\n')
            file.write(f'#define DATA_TYPE_BYTE              1\n')
            file.write(f'typedef uint8_t                     {header_prefix.lower()}_data_t;\n')
            if numerical_check == 1:
                file.write(f'#define REDMULE_COMPUTE_TYPE        REDMULE_FP_8\n')
                file.write(f'#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_FP_8\n')
                file.write(f'#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_FP_8\n')
            else:
                file.write(f'#define REDMULE_COMPUTE_TYPE        REDMULE_UINT_8\n')
                file.write(f'#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_NONE\n')
                file.write(f'#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_NONE\n')
        
        file.write('#define STR(x) #x\n')
        file.write('#define XSTR(x) STR(x)\n')
        file.write(f'\n#endif // _{header_prefix.upper()}_CONFIG_H_\n')
    print(f'Header file "{C_header_file}" generated successfully.')
    pass
