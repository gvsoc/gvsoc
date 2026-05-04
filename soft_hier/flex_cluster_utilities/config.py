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
            clog2_stack_offest_per_core = int(math.ceil(math.log2(int(attr_value, 16)/(1 << (num_core_per_cluster - 1).bit_length()))))
            file.write(f'.set ARCH_CLUSTER_STACK_OFFSET, {clog2_stack_offest_per_core}\n')
            pass
        file.write(f'.set {define_name}, {attr_value}\n')
    
    file.write('\n#endif // FLEXCLUSTERARCH_H\n')

print(f'Header file "{S_header_file}" generated successfully.')

#Generate the memory.ld file
def generate_memory_ld(
        l1_base,
        l1_size,
        l3_base,
        l3_size,
        hbm_west_base,
        hbm_west_size,
        hbm_north_base,
        hbm_north_size,
        hbm_east_base,
        hbm_east_size,
        hbm_south_base,
        hbm_south_size):
    l1_share_size = 0x200
    ld_content = f"""
MEMORY
{{
    L1_SHARE  (rwxa) : ORIGIN = 0x{(l1_base):016X}, LENGTH = 0x{(l1_share_size):016X}
    L1  (rwxa) : ORIGIN = 0x{(l1_base + l1_share_size):016X}, LENGTH = 0x{(l1_size - l1_share_size):016X}
    L3  (rwxa) : ORIGIN = 0x{l3_base:016X}, LENGTH = 0x{l3_size:016X}
    HBM (rwxa) : ORIGIN = 0x{hbm_west_base:016X}, LENGTH = 0x{hbm_west_size:016X}
    HBM_WEST  (rwxa) : ORIGIN = 0x{hbm_west_base:016X}, LENGTH = 0x{hbm_west_size:016X}
    HBM_NORTH (rwxa) : ORIGIN = 0x{hbm_north_base:016X}, LENGTH = 0x{hbm_north_size:016X}
    HBM_EAST  (rwxa) : ORIGIN = 0x{hbm_east_base:016X}, LENGTH = 0x{hbm_east_size:016X}
    HBM_SOUTH (rwxa) : ORIGIN = 0x{hbm_south_base:016X}, LENGTH = 0x{hbm_south_size:016X}
}}
"""
    return ld_content
with open('soft_hier/flex_cluster_sdk/runtime/memory.ld', 'w') as file:
    num_cluster_x = 0
    num_cluster_y = 0
    cluster_tcdm_base = 0
    cluster_tcdm_size = 0
    instruction_mem_base = 0
    instruction_mem_size = 0
    hbm_start_base = 0
    hbm_node_addr_space = 0
    for attr_name, attr_value in attributes.items():
        if attr_name == 'num_cluster_x':
            num_cluster_x = int(attr_value)
        if attr_name == 'num_cluster_y':
            num_cluster_y = int(attr_value)
        if attr_name == 'cluster_tcdm_base':
            cluster_tcdm_base = int(attr_value, 16)
        if attr_name == 'cluster_tcdm_size':
            cluster_tcdm_size = int(attr_value, 16)
        if attr_name == 'instruction_mem_base':
            instruction_mem_base = int(attr_value, 16)
        if attr_name == 'instruction_mem_size':
            instruction_mem_size = int(attr_value, 16)
        if attr_name == 'hbm_start_base':
            hbm_start_base = int(attr_value, 16)
        if attr_name == 'hbm_node_addr_space':
            hbm_node_addr_space = int(attr_value, 16)
    l1_base = cluster_tcdm_base
    l1_size = cluster_tcdm_size
    l3_base = instruction_mem_base
    l3_size = instruction_mem_size
    hbm_west_base = hbm_start_base
    hbm_west_size = num_cluster_y * hbm_node_addr_space
    hbm_north_base = hbm_west_base + hbm_west_size
    hbm_north_size = num_cluster_x * hbm_node_addr_space
    hbm_east_base = hbm_north_base + hbm_north_size
    hbm_east_size = num_cluster_y * hbm_node_addr_space
    hbm_south_base = hbm_east_base + hbm_east_size
    hbm_south_size = num_cluster_x * hbm_node_addr_space
    ld_content = generate_memory_ld(
        l1_base,
        l1_size,
        l3_base,
        l3_size,
        hbm_west_base,
        hbm_west_size,
        hbm_north_base,
        hbm_north_size,
        hbm_east_base,
        hbm_east_size,
        hbm_south_base,
        hbm_south_size)
    file.write(ld_content)

print('Linker script "memory.ld" generated successfully.')
