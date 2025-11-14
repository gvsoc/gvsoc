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

class Router(gvsoc.systree.Component):
    def __init__(self, parent, name, radix, virtual_ch):
        super(Router, self).__init__(parent, name)
        self.add_property('radix', radix)
        self.add_property('virtual_ch', virtual_ch)
        self.add_sources(['pulp/c2c_platform/router.cpp'])

    def i_PORT_INPUT(self, i: int) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'in_{i}', signature='io')

    def o_PORT_OUT(self, itf: gvsoc.systree.SlaveItf, i: int):
        self.itf_bind(f'out_{i}', itf, signature='io')

    def o_TOP_OUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'top_req', itf, signature='io')
