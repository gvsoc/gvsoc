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

class HWPEInterleaver(gvsoc.systree.Component):

    def __init__(self, parent, slave, nb_master_ports, nb_banks, bank_width):

        super(HWPEInterleaver, self).__init__(parent, slave)

        self.add_sources(
            ['pulp/chips/flex_cluster/hwpe_interleaver.cpp']
        )

        self.add_properties({
            'nb_banks': nb_banks,
            'bank_width': bank_width
        })
