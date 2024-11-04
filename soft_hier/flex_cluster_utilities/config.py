import re

# Read the input Python file
input_file = 'pulp/pulp/chips/flex_cluster/flex_cluster_arch.py'
output_file = 'soft_hier/flex_cluster_sdk/test/include/flex_cluster_arch.h'

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
with open(output_file, 'w') as file:
    file.write('#ifndef FLEXCLUSTERARCH_H\n')
    file.write('#define FLEXCLUSTERARCH_H\n\n')
    
    for attr_name, attr_value in attributes.items():
        # Convert attribute name to uppercase and prefix with 'ARCH_'
        define_name = f'ARCH_{attr_name.upper()}'
        file.write(f'#define {define_name} {attr_value}\n')
    
    file.write('\n#endif // FLEXCLUSTERARCH_H\n')

print(f'Header file "{output_file}" generated successfully.')
