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

        #Cluster
        self.num_cluster_x           = 1
        self.num_cluster_y           = 1
        self.num_core_per_cluster    = 2

        self.cluster_tcdm_bank_width = 8
        self.cluster_tcdm_bank_nb    = 32

        self.cluster_tcdm_base       = 0x00000000
        self.cluster_tcdm_size       = 0x00020000
        self.cluster_tcdm_remote     = 0x30000000

        self.cluster_stack_base      = 0x10000000
        self.cluster_stack_size      = 0x00020000

        self.cluster_reg_base        = 0x20000000
        self.cluster_reg_size        = 0x00000200

        #RedMule
        self.redmule_ce_height       = 4
        self.redmule_ce_width        = 1
        self.redmule_ce_pipe         = 3
        self.redmule_elem_size       = 2
        self.redmule_queue_depth     = 32
        self.redmule_reg_base        = 0x20020000
        self.redmule_reg_size        = 0x00000200

        #HBM
        self.hbm_start_base          = 0xc0000000
        self.hbm_node_interleave     = 0x00010000
        self.num_hbm_ch_per_node     = 1
        self.hbm_placement           = [1,0,0,0]

        #NoC
        self.noc_outstanding         = 2
        self.noc_link_width          = 512

        #System
        self.instruction_mem_base    = 0x80000000
        self.instruction_mem_size    = 0x00010000

        self.soc_register_base       = 0x90000000
        self.soc_register_size       = 0x00010000
        self.soc_register_eoc        = 0x90000000
        self.soc_register_wakeup     = 0x90000004

        #Synchronization
        self.sync_base               = 0x40000000
        self.sync_interleave         = 0x00000010
