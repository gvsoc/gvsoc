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

    def __init__(self, parent: gvsoc.systree.Component, name: str):

        super().__init__(parent, name)

        self.add_sources(['pulp/chips/flex_cluster/ctrl_registers.cpp'])

        self.add_properties({
        })

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')
    
    def i_BARRIER_ACK(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'barrier_ack', signature='wire<bool>')

    def o_BARRIER_ACK(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('barrier_ack', itf, signature='wire<bool>')