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

import os
import io
import re
import sys
import math
import torch
import shutil
import argparse
import numpy as np
import importlib.util
from tqdm import tqdm

def is_power_of_two(name, value):
    if value <= 0 or (value & (value - 1)) != 0:
        raise AssertionError(f"The value of '{name}' must be a power of two, but got {value}.")
    return True

def max_divisor(M, K):
    for d in range(K, 0, -1):  # start from K and go down
        if M % d == 0:
            return d

def opt(moex, arch):
    elem_size = 1 if moex.dtype == 'fp8' else 2
    #3 x RoutedE + 3 Emb
    if not hasattr(moex, 'embedded_length'):
        maximum_token_per_cluster = math.floor(arch.cluster_tcdm_size / (3 * moex.num_routed_experts * elem_size))
    else:
        assert 3 * moex.embedded_length * elem_size < arch.cluster_tcdm_size
        maximum_token_per_cluster = math.floor((arch.cluster_tcdm_size - 3 * moex.embedded_length * elem_size) / (3 * moex.num_routed_experts * elem_size))
        pass
    assert maximum_token_per_cluster > 0
    num_cluster =  arch.num_cluster_x * arch.num_cluster_y
    average_token_per_cluster = (moex.num_tokens + num_cluster - 1) // num_cluster
    moex.token_per_cluster = min(average_token_per_cluster, maximum_token_per_cluster)
    return moex
    pass
