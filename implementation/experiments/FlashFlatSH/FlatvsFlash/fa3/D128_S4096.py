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

class FlatAttetion:

    def __init__(self):

        #Attention parameters
        self.dtype                   = 'fp16'
        self.kv_sequence_length      = 4096
        self.q_sequence_length       = 256
        self.speculative_length      = 1
        self.head_dimemsion          = 128
        self.num_head                = 32
        self.num_head_group          = 32
        self.batch_size              = 32

        #Flatten Settings
        self.use_flash_attention     = 1
        ## [Shape]: Attention matrix shape (x=shape, y=shape) that a Group need to handle for each iteration
        self.flatten_shape_x         = 128
        self.flatten_shape_y         = 128
        ## [Async]: Whether to enable asynchronous execution
        self.flatten_async           = 1
        ## [Numer]: Whether to check numerical correctness
        self.flatten_numer           = 0
        self.flatten_numer_chunk     = 8192
