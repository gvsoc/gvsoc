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

class PlatformCtrl(gvsoc.systree.Component):
    def __init__(self, parent, name, num_chip):
        #Initialize the parent class
        super(PlatformCtrl, self).__init__(parent, name)

        # Add sources
        self.add_sources(['pulp/c2c_platform/platformctrl.cpp'])

        self.add_properties({
            'num_chip': num_chip,
        })

    def i_BARRIER_ACK(self, i : int) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'barrier_ack_{i}', signature='wire<bool>')

    def o_START(self, itf: gvsoc.systree.SlaveItf, i : int):
        self.itf_bind(f'start_{i}', itf, signature='wire<bool>')
