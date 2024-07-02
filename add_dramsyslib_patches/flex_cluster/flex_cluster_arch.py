#
# Copyright (C) 2020 ETH Zurich and University of Bologna
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

# Author: Chi Zhang <chizhang@ethz.ch>

class FlexClusterArch:

    def __init__(self):
        self.num_cluster_x           = 2
        self.num_cluster_y           = 2
        self.noc_link_width          = 512
        self.num_core_per_cluster    = 10

        self.instruction_mem_base    = 0x8000_0000
        self.instruction_mem_size    = 0x0001_0000

        self.cluster_tcdm_base       = 0x0000_0000
        self.cluster_tcdm_size       = 0x0002_0000

        self.cluster_stack_base      = 0x1000_0000

        self.soc_register_base       = 0x9000_0000
        self.soc_register_size       = 0x0001_0000

        self.hbm_start_base          = 0xc000_0000
         # meta parameters
        self.num_clusters            = self.num_cluster_x * self.num_cluster_y
