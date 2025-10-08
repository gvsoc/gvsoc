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

class Model:

    def __init__(self):

        #Basic Configuration
        self.model_name                 = "Qwen"
        self.dtype                      = 'fp16'
        self.num_layers                 = 32
        self.embeded_length             = 4096
        self.max_sequence_length        = 8192

        #Attention Configuration
        self.attention_type             = 'MHA'
        self.num_heads                  = 32
        self.head_dimension             = 128
        self.qk_rope_enable             = 1
        self.head_groups                = 32

        #FFN Configuration
        self.ffn_type                   = 'MLP'
        self.mlp_inter_dim              = 22016
        self.mlp_acti_algo              = 'silu'
        self.mlp_acti_bias_enable       = 0
        