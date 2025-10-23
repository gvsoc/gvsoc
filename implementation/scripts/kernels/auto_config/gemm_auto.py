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

def opt(gemm, arch):
    #1. Find Tile Size
    m_tile = 4 * arch.redmule_ce_height
    n_tile = 4 * arch.redmule_ce_height
    k_tile = 4 * arch.redmule_ce_height
    m_tile = gemm.m_size if gemm.m_size < m_tile else m_tile
    n_tile = gemm.n_size if gemm.n_size < n_tile else n_tile
    assert(gemm.m_size % m_tile == 0)
    assert(gemm.n_size % n_tile == 0)

    #2. Determine Group Scale
    scale_y = gemm.m_size // m_tile
    scale_x = gemm.n_size // n_tile
    is_power_of_two("scale_x", scale_x)
    is_power_of_two("scale_y", scale_y)
    scale_y = arch.num_cluster_y if scale_y > arch.num_cluster_y else scale_y
    scale_x = arch.num_cluster_x if scale_x > arch.num_cluster_x else scale_x

    #3. Determine Split K
    group_number = 1
    group_reduce = 0
    group_splitk = 0
    group_gap_x = 0
    group_gap_w = 0
    group_gap_z = 0
    available_group = (arch.num_cluster_y * arch.num_cluster_x) // (scale_y * scale_x)
    try_splitK = 2
    while try_splitK <= available_group:
        if gemm.k_size % try_splitK == 0:
            group_number = try_splitK
        else:
            break
            pass
        try_splitK *= 2
        pass
    if group_number > 1:
        splitedK = gemm.k_size // group_number
        k_line = 4 * arch.redmule_ce_height
        if splitedK <= k_line:
            k_tile = splitedK
        else:
            md = max_divisor(splitedK, k_line)
            k_tile = max(md, int(k_line * 3/4)) if (splitedK // k_line) < 4 else k_line
            pass
        assert(gemm.k_size % k_tile == 0)
        group_reduce = 1
        group_splitk = 1
        elem_size = 1 if gemm.dtype == 'fp8' else 2
        group_gap_x = splitedK * elem_size
        group_gap_w = splitedK * gemm.n_size * elem_size
        group_gap_z = 0
        pass

    gemm.m_tile                  = m_tile
    gemm.n_tile                  = n_tile
    gemm.k_tile                  = k_tile
    gemm.summa_scale_x           = scale_x
    gemm.summa_scale_y           = scale_y
    gemm.summa_group_number      = group_number
    gemm.summa_group_reduce      = group_reduce
    gemm.summa_group_splitk      = group_splitk
    gemm.summa_group_gap_x       = group_gap_x
    gemm.summa_group_gap_w       = group_gap_w
    gemm.summa_group_gap_z       = group_gap_z

    return gemm
    pass
