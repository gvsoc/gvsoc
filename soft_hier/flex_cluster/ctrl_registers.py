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

import gvsoc.systree

class CtrlRegisters(gvsoc.systree.Component):

    def __init__(self, parent: gvsoc.systree.Component, name: str, num_cluster_x: int, num_cluster_y: int, has_preload_binary: int=0):

        super().__init__(parent, name)

        self.add_sources(['pulp/chips/flex_cluster/ctrl_registers.cpp'])

        self.add_properties({
            'num_cluster_x': num_cluster_x,
            'num_cluster_y': num_cluster_y,
            'has_preload_binary': has_preload_binary,
        })

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')

    def i_HBM_PRELOAD_DONE(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'hbm_preload_done', signature='wire<bool>')

    def o_HBM_PRELOAD_DONE_TO_CLUSTER(self, itf: gvsoc.systree.SlaveItf, i):
        self.itf_bind(f'hbm_preload_done_to_cluster_{i}', itf, signature='wire<bool>')
