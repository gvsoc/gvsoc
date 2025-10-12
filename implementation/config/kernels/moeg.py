#
# Copyright (C) 2025 ETH Zurich and University of Bologna
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

class MoEGate:

    def __init__(self):

        #Activation parameters
        ## Algorithm support with : sigmoid, relu, silu, none
        self.dtype                   = 'fp16'
        self.num_tokens              = 1024
        self.num_routed_experts      = 256
        self.num_active_experts      = 8

        #HyperParameters
        ## [Token]: number of token to be processed within a cluster
        self.token_per_cluster       = 4
        ## [Numer]: Whether to check numerical correctness
        self.moeg_numer              = 1
        self.moeg_numer_chunk        = 8192
