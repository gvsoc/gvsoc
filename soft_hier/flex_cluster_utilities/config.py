import re
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
    
    for attr_name, attr_value in attributes.items():
        # Convert attribute name to uppercase and prefix with 'ARCH_'
        define_name = f'ARCH_{attr_name.upper()}'
        file.write(f'#define {define_name} {attr_value}\n')
    
    file.write('\n#endif // FLEXCLUSTERARCH_H\n')

print(f'Header file "{C_header_file}" generated successfully.')

# Write the output S header file
with open(S_header_file, 'w') as file:
    file.write('#ifndef FLEXCLUSTERARCH_H\n')
    file.write('#define FLEXCLUSTERARCH_H\n\n')
    
    for attr_name, attr_value in attributes.items():
        # Convert attribute name to uppercase and prefix with 'ARCH_'
        define_name = f'ARCH_{attr_name.upper()}'
        if define_name == 'ARCH_HBM_PLACEMENT':
            continue
            pass
        file.write(f'.set {define_name}, {attr_value}\n')
    
    file.write('\n#endif // FLEXCLUSTERARCH_H\n')

print(f'Header file "{S_header_file}" generated successfully.')
