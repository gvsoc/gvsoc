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
        self.sequence_length         = 512
        self.head_dimemsion          = 128
        self.num_head                = 32
        self.batch_size              = 1
        self.elem_size               = 2

        #Flatten Settings
        ## [Scale]: How many clusters (x=scale, y=scale) are assigned for one head.
        ##          For a set of clusters (x=scale, y=scale) we would call it **Group**
        self.flatten_scale           = 4
        ## [Shape]: Attention matrix shape (x=shape, y=shape) that a Group need to handle for each iteration
        self.flatten_shape           = 512
        ## [Layot]: Which data layout scheme is used
        ##          - "tiled      QOKV": QO in west HBM, KV in south HBM
        ##          - "tiled      QKOV": QK in west HBM, OV in south HBM
        ##          - "tiled      QVOK": QV in west HBM, OK in south HBM
        ##          - "tiled      OKQV": OK in west HBM, QV in south HBM
        ##          - "tiled      OVQK": OV in west HBM, QK in south HBM
        ##          - "tiled      KVQO": KV in west HBM, QO in south HBM
        self.flatten_layot_method    = "QOKV"
        ## [Async]: Whether to enable asynchronous execution
        self.flatten_async           = 1
        ## [Numer]: Whether to check numerical correctness
        self.flatten_numerical_check = 0
