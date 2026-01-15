import re
import ast
import math
import argparse

parser = argparse.ArgumentParser(description="Generate C and S header files from a SoftHier configuration file.")
parser.add_argument("input_file", nargs="?", default="soft_hier/flex_cluster/flex_cluster_arch.py", help="Path to the input Python file")
args = parser.parse_args()
input_file = args.input_file

# Read the input Python file
C_header_file = 'soft_hier/flex_cluster_sdk/runtime/include/flex_cluster_arch.h'
S_header_file = 'soft_hier/flex_cluster_sdk/runtime/include/flex_cluster_arch.inc'

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
    file.write('#ifndef FLEXCLUSTERARCH_H\n')
    file.write('#define FLEXCLUSTERARCH_H\n\n')
    num_core_per_cluster = 0
    
    for attr_name, attr_value in attributes.items():
        # Convert attribute name to uppercase and prefix with 'ARCH_'
        define_name = f'ARCH_{attr_name.upper()}'
        if define_name == 'ARCH_NUM_CORE_PER_CLUSTER':
            num_core_per_cluster = int(attr_value)
            pass
        if define_name == 'ARCH_SPATZ_ATTACED_CORE_LIST':
            core_list = ast.literal_eval(attr_value)
            file.write(f'#define ARCH_SPATZ_ATTACED_CORES {len(core_list)}\n')
            attach_list = []
            sid_list = []
            sid = 0
            for x in range(num_core_per_cluster):
                if x in core_list:
                    attach_list.append(1)
                    sid_list.append(sid)
                    sid = sid + 1
                else:
                    attach_list.append(0)
                    sid_list.append(0)
                    pass
                pass
            attach_list_str = str(attach_list).replace("[", "{").replace("]", "}")
            file.write(f'#define ARCH_SPATZ_ATTACED_CHECK_LIST {attach_list_str}\n')
            sid_list_str = str(sid_list).replace("[", "{").replace("]", "}")
            file.write(f'#define ARCH_SPATZ_ATTACED_SID_LIST {sid_list_str}\n')
            attr_value = attr_value.replace("[", "{").replace("]", "}")
            pass
        file.write(f'#define {define_name} {attr_value}\n')
    
    file.write('\n#endif // FLEXCLUSTERARCH_H\n')

print(f'Header file "{C_header_file}" generated successfully.')

# Write the output S header file
with open(S_header_file, 'w') as file:
    file.write('#ifndef FLEXCLUSTERARCH_H\n')
    file.write('#define FLEXCLUSTERARCH_H\n\n')
    num_core_per_cluster = 0
    
    for attr_name, attr_value in attributes.items():
        # Convert attribute name to uppercase and prefix with 'ARCH_'
        define_name = f'ARCH_{attr_name.upper()}'
        if define_name == 'ARCH_NUM_CORE_PER_CLUSTER':
            num_core_per_cluster = int(attr_value)
            pass
        if define_name == 'ARCH_HBM_CHAN_PLACEMENT' or define_name == 'ARCH_SPATZ_ATTACED_CORE_LIST' or define_name == 'ARCH_HBM_TYPE':
            continue
            pass
        if define_name == 'ARCH_CLUSTER_STACK_SIZE':
            clog2_stack_offest_per_core = min(int(math.ceil(math.log2(int(attr_value, 16)/num_core_per_cluster))), 14)
            file.write(f'.set ARCH_CLUSTER_STACK_OFFSET, {clog2_stack_offest_per_core}\n')
            pass
        file.write(f'.set {define_name}, {attr_value}\n')
    
    file.write('\n#endif // FLEXCLUSTERARCH_H\n')

print(f'Header file "{S_header_file}" generated successfully.')
