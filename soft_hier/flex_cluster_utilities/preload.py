import os
import numpy as np

# Map NumPy types to C types
NP_DTYPE_TO_C = {
    np.dtype('int8'): 'int8_t',
    np.dtype('uint8'): 'uint8_t',
    np.dtype('int16'): 'int16_t',
    np.dtype('uint16'): 'uint16_t',
    np.dtype('int32'): 'int32_t',
    np.dtype('uint32'): 'uint32_t',
    np.dtype('int64'): 'int64_t',
    np.dtype('uint64'): 'uint64_t',
    np.dtype('float16'): '_Float16',
    np.dtype('float32'): 'float',
    np.dtype('float64'): 'double',
}

def make_preload_elf(output_file_path, np_arrays, start_addresses=None):
    """
    Generate an ELF file preloading numpy arrays.

    Parameters:
    - output_file_path (str): Path to save the output ELF file.
    - np_arrays (list of numpy.ndarray): List of numpy arrays to include in the ELF.
    - start_addresses (list of int or None): List of starting addresses for each array, or None.
      If None, addresses are auto-determined with 64-byte alignment.
    """
    # Handle default for start_addresses
    if start_addresses is None:
        start_addresses = [None] * len(np_arrays)

    # Validate inputs
    if len(np_arrays) != len(start_addresses):
        raise ValueError("np_arrays and start_addresses must have the same length.")

    # 64-byte alignment
    alignment = 64
    current_address = 0xc0000000  # Default starting address for auto-addressing

    # Step 1: Create "array.c"
    array_c_content = ['#include <stdint.h>']
    section_names = []

    for idx, (array, start_addr) in enumerate(zip(np_arrays, start_addresses)):
        # Determine C type from NumPy dtype
        c_type = NP_DTYPE_TO_C.get(array.dtype, None)
        if c_type is None:
            raise TypeError(f"Unsupported NumPy dtype: {array.dtype}")

        section_name = f".custom_section_{idx}"
        section_names.append(section_name)

        if start_addr is None:
            # Auto-determine the address with alignment
            start_addr = (current_address + alignment - 1) & ~(alignment - 1)
        else:
            # Ensure provided addresses are aligned
            if start_addr % alignment != 0:
                raise ValueError(f"Provided address {start_addr} is not {alignment}-byte aligned.")

        # Generate the array definition
        array_values = ", ".join(map(str, array.flatten()))
        array_c_content.append(
            f'{c_type} array_{idx}[] __attribute__((section("{section_name}"))) = {{{array_values}}};'
        )

        current_address = start_addr + array.nbytes

    array_c_code = "\n".join(array_c_content)

    with open("array.c", "w") as f:
        f.write(array_c_code)


    # Step 2: Create "link.ld"
    link_ld_content = ["SECTIONS {"]
    current_address = 0xc0000000  # Reset for linker script auto-addressing

    for idx, (array, start_addr) in enumerate(zip(np_arrays, start_addresses)):
        section_name = section_names[idx]

        if start_addr is None:
            # Auto-determine the address with alignment
            start_addr = (current_address + alignment - 1) & ~(alignment - 1)
        link_ld_content.append(
            f"    . = 0x{start_addr:X};\n    {section_name} : {{ *({section_name}) }}"
        )
        current_address = start_addr + array.nbytes

    link_ld_content.append("}")
    link_ld_code = "\n".join(link_ld_content)

    with open("link.ld", "w") as f:
        f.write(link_ld_code)

    # Step 3: Compile the ELF file
    os.system("riscv32-unknown-elf-gcc -c array.c -o array.o")
    os.system(f"riscv32-unknown-elf-ld -T link.ld array.o -o {output_file_path}")
    os.system(f"riscv32-unknown-elf-strip --remove-section=.comment --remove-section=.Pulp_Chip.Info {output_file_path}")

    # Step 4: Cleanup
    os.remove("array.c")
    os.remove("link.ld")
    os.remove("array.o")

if __name__ == "__main__":
    # Example usage
    np_arrays = [
        np.array([1, 2, 3], dtype=np.int32),
        np.array([4, 5, 6, 7, 8], dtype=np.int32),
    ]
    # Automatic alignment for all arrays
    make_preload_elf("output.elf", np_arrays)
