#
# Copyright (C) 2024 ETH Zurich and University of Bologna
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

# Author: Chi Zhang <chizhang@iis.ee.ethz.ch>

import gvsoc.systree

def get_gemm_tile_pJ(num_tile_mac, tech_node):
    gemm_tile_pJ ={
        "22nm" : {
            "25": {
                "0.9": {
                    "any": 4 * num_tile_mac
                },
                "1.0": {
                    "any": 4 * num_tile_mac
                }
            }
        },
        "12nm" : {
            "25": {
                "0.8": {
                    "any": 2.4 * num_tile_mac
                },
                "0.9": {
                    "any": 2.4 * num_tile_mac
                }
            }
        },
        "7nm" : {
            "25": {
                "0.7": {
                    "any": 1.6 * num_tile_mac
                },
                "0.8": {
                    "any": 1.6 * num_tile_mac
                }
            }
        },
        "5nm" : {
            "25": {
                "0.6": {
                    "any": 1.2 * num_tile_mac
                },
                "0.7": {
                    "any": 1.2 * num_tile_mac
                }
            }
        }
    }

    return gemm_tile_pJ[tech_node]
    pass

class LightRedmule(gvsoc.systree.Component):

    def __init__(self,
                parent: gvsoc.systree.Component,
                name: str,
                tcdm_bank_width: int,
                tcdm_bank_number: int,
                elem_size: int,
                ce_height: int,
                ce_width: int,
                ce_pipe: int,
                queue_depth: int=128,
                fold_tiles_mapping: int=0,
                tech_node: str="5nm"):

        super().__init__(parent, name)

        self.add_sources(['pulp/chips/flex_cluster/light_redmule.cpp'])

        self.add_properties({
            'tcdm_bank_width'   : tcdm_bank_width,
            'tcdm_bank_number'  : tcdm_bank_number,
            'elem_size'         : elem_size,
            'ce_height'         : ce_height,
            'ce_width'          : ce_width,
            'ce_pipe'           : ce_pipe,
            'queue_depth'       : queue_depth,
            'fold_tiles_mapping': fold_tiles_mapping,
        })

        self.LOCAL_BUFFER_H    = ce_height;
        self.LOCAL_BUFFER_N    = tcdm_bank_width * tcdm_bank_number // elem_size;
        self.LOCAL_BUFFER_W    = ce_width * (ce_pipe + 1);

        self.add_properties({
            "gemm_tile_energy": {
                "dynamic": {
                    "type": "linear",
                    "unit": "pJ",
                    "values": get_gemm_tile_pJ(self.LOCAL_BUFFER_H * self.LOCAL_BUFFER_W * self.LOCAL_BUFFER_N, tech_node),
                }
            }
        })

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')

    def i_CORE_ACC(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'core_acc', signature='io')

    def o_TCDM(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('tcdm', itf, signature='io')