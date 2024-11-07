"""
Helper for using RTL simulation testbench on gap
"""

#
# Copyright (C) 2022 GreenWaves Technologies
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

#
# Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
#

import os
from elftools.elf.elffile import ELFFile

def __gen_stim_slm(filename, mem, width, stim_format=None):

    #self.dump('  Generating to file: ' + filename)

    os.makedirs(os.path.dirname(filename), exist_ok=True)

    with open(filename, 'w', encoding='utf-8') as file:
        for key in sorted(mem.keys()):
            if stim_format == 'slm':
                file.write('@%X %0*X\n' % (int(key), width*2, mem.get(key)))
            else:
                file.write('%X_%0*X\n' % (int(key), width*2, mem.get(key)))



def __add_mem_word(mem, base, size, data, width):

    aligned_base = base & ~(width - 1)

    shift = base - aligned_base
    iter_size = width - shift
    iter_size = min(iter_size, size)

    value = mem.get(str(aligned_base))
    if value is None:
        value = 0

    value &= ~(((1<<width) - 1) << (shift*8))
    value |= int.from_bytes(data[0:iter_size], byteorder='little') << (shift*8)

    mem[str(aligned_base)] = value

    return iter_size


def __add_mem(mem, base, size, data, width):

    while size > 0:

        iter_size = __add_mem_word(mem, base, size, data, width)

        size -= iter_size
        base += iter_size
        data = data[iter_size:]

def __parse_binaries(width, binaries):

    mem = {}

    for binary in binaries:

        with open(binary, 'rb') as file:
            elffile = ELFFile(file)

            for segment in elffile.iter_segments():

                if segment['p_type'] == 'PT_LOAD':

                    data = segment.data()
                    addr = segment['p_paddr']
                    size = len(data)

                    #self.dump('  Handling section (base: 0x%x, size: 0x%x)' % (addr, size))

                    __add_mem(mem, addr, size, data, width)

                    if segment['p_filesz'] < segment['p_memsz']:
                        addr = segment['p_paddr'] + segment['p_filesz']
                        size = segment['p_memsz'] - segment['p_filesz']
                        #self.dump('  Init section to 0 (base: 0x%x, size: 0x%x)' % (addr, size))
                        __add_mem(mem, addr, size, [0] * size, width)

    return mem

def gen_jtag_stimuli(binary: str, stim_file: str):
    """Generate stimuli for JTAG loading

    The RTL testbench needs a preload file for the memory generated from the test binany.
    The testbench will load the preload file through JTAG.

    Parameters
    ----------
    binary : str
        The path to the test binary to be loaded.
    stim_file : str
        The path to stimuli file to be generated
    """

    memory = __parse_binaries(4, [binary])
    __gen_stim_slm(stim_file, memory, 4, stim_format='slm')
