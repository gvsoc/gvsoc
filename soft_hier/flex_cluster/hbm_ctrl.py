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

import gvsoc.systree as st

class hbm_ctrl(st.Component):

    def __init__(self, parent, slave, nb_slaves=0, nb_masters=0, stage_bits=0, interleaving_bits=2, node_addr_offset=0):

        super(hbm_ctrl, self).__init__(parent, slave)

        self.add_sources(['pulp/chips/flex_cluster/hbm_ctrl.cpp'])

        self.add_properties({
            'nb_slaves': nb_slaves,
            'nb_masters': nb_masters,
            'stage_bits': stage_bits,
            'interleaving_bits': interleaving_bits,
            'node_addr_offset': node_addr_offset
        })
