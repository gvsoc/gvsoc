#
# Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
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

import gvsoc.systree

access_byte_pJ ={
    "22nm" : {
        "25": {
            "0.9": {
                "any": 0.52
            },
            "1.0": {
                "any": 0.64
            }
        }
    },
    "12nm" : {
        "25": {
            "0.8": {
                "any": 0.38
            },
            "0.9": {
                "any": 0.48
            }
        }
    },
    "7nm" : {
        "25": {
            "0.7": {
                "any": 0.25
            },
            "0.8": {
                "any": 0.32
            }
        }
    },
    "5nm" : {
        "25": {
            "0.6": {
                "any": 0.15
            },
            "0.7": {
                "any": 0.20
            }
        }
    }
}

def get_leakage_byte_W(bytes, tech_node):
    leakage_byte_W ={
        "22nm" : {
            "25": {
                "0.9": {
                    "any": 100 * 1e-12 * bytes
                },
                "1.0": {
                    "any": 120 * 1e-12 * bytes
                }
            }
        },
        "12nm" : {
            "25": {
                "0.8": {
                    "any": 60 * 1e-12 * bytes
                },
                "0.9": {
                    "any": 80 * 1e-12 * bytes
                }
            }
        },
        "7nm" : {
            "25": {
                "0.7": {
                    "any": 40 * 1e-12 * bytes
                },
                "0.8": {
                    "any": 50 * 1e-12 * bytes
                }
            }
        },
        "5nm" : {
            "25": {
                "0.6": {
                    "any": 45 * 1e-12 * bytes
                },
                "0.7": {
                    "any": 40 * 1e-12 * bytes
                }
            }
        }
    }

    return leakage_byte_W[tech_node]
    pass

class Memory(gvsoc.systree.Component):
    """Memory array

    This models a simple memory model.
    It can be preloaded with initial data.
    It contains a timing model of a bandwidth, reported through latency.
    It can support riscv atomics.

    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    size: int
        The size of the memory in bytes.
    width_log2: int
        The log2 of the bandwidth to the memory, i.e. the number of bytes it can transfer per cycle.
        No timing model is applied if it is zero and the memory is then having an infinite
        bandwidth.
    stim_file: str
        The path to a binary file which should be preloaded at beginning of the memory. The format
        is a raw binary, and is loaded with an fread.
    power_trigger: bool
        True if the memory should trigger power report generation based on dedicated accesses.
    align: int
        Specify a required alignment for the allocated memory used for the memory model.
    atomics: bool
        True if the memory should support riscv atomics. Since this is slowing down the model, it
        should be set to True only if needed.
    latency: int
        Specify extra latency which will be added to any incoming request.
    memcheck_id: int
        If this memory is used to track buffer overflow, this gives the global memory check id.
    memcheck_base: int
        Absolute base of memory where buffer overflow is tracked.
    memcheck_virtual_base: int
        Absolute virtual base of allocated buffers.
    memcheck_expansion_factor: int
        Extra size used to track buffer overflow.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str, size: int, width_log2: int=2,
            stim_file: str=None, power_trigger: bool=False,
            align: int=0, atomics: bool=False, latency=0, memcheck_id: int=-1, memcheck_base: int=0,
            memcheck_virtual_base: int=0, memcheck_expansion_factor: int=5, tech_node: str="5nm"):

        super().__init__(parent, name)

        self.add_sources(['pulp/chips/flex_cluster/memory.cpp'])

        # Since atomics are slowing down the model, this is better to compile the support only
        # if needed. Note that the framework will take care of compiling this model twice
        # if both memories with and without atomics are instantiated.
        if atomics:
            self.add_c_flags(['-DCONFIG_ATOMICS=1'])

        self.add_properties({
            'size': size,
            'stim_file': stim_file,
            'power_trigger': power_trigger,
            'width_bits': width_log2,
            'align': align,
            'latency': latency,
            'memcheck_id': memcheck_id,
            'memcheck_base': memcheck_base,
            'memcheck_virtual_base': memcheck_virtual_base,
            'memcheck_expansion_factor': memcheck_expansion_factor,
            'tech_node': tech_node
        })

        self.add_properties({
            "background": {
                "leakage": {
                    "type": "linear",
                    "unit": "W",

                    "values": get_leakage_byte_W(size, tech_node)
                },
            },
            "access_byte": {
                "dynamic": {
                    "type": "linear",
                    "unit": "pJ",
                    "values": access_byte_pJ[tech_node],
                }
            }
        })


    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        """Returns the input port.

        Incoming requests to be handled by the memory should be sent to this port.\n
        It instantiates a port of type vp::IoSlave.\n

        Returns
        ----------
        gvsoc.systree.SlaveItf
            The slave interface
        """
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')
