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

class C2CPlatformCFG:

    def __init__(self):

        # Platform
        self.num_chip               = 16
        self.use_trace_file         = 0
        self.trace_file_base        = ''

        # Links
        self.link_latency_ns        = 256
        self.local_latency_ns       = 3
        self.link_bandwidth_GBps    = 256
        self.link_depth_rx          = 1024
        self.link_depth_tx          = 1024
        self.link_credit_bar        = 10
        self.flit_granularity_byte  = 512

        # Topology - SelfLink
        # self.topology               = "self-link"

        # Topology - Mesh2D
        self.topology               = "mesh2d"
        self.num_chip_x             = 4
        self.num_chip_y             = 4

        # Topology - FatTree
        # self.topology               = "fattree"
        # self.radix                  = 4
        # self.level                  = 3
