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
        self.model_name                 = "DeepSeekV3-671B"
        self.dtype                      = 'fp16'
        self.num_layers                 = 61
        self.embeded_length             = 7186
        self.max_sequence_length        = 8192

        #Attention Configuration
        self.attention_type             = 'MLA'
        self.num_heads                  = 128
        self.head_dimension             = 128
        self.q_lora_rank                = 1536
        self.kv_lora_rank               = 512
        self.rope_head_dim              = 64

        #MoE Configuration
        self.ffn_type                   = 'MoE'
        self.moe_inter_dim              = 2048
        self.n_routed_experts           = 256
        self.n_shared_experts           = 1
        self.n_activated_experts        = 8
        