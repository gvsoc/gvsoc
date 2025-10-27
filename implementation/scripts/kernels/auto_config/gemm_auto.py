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
import copy
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

def next_power_of_2(n: int) -> int:
    if n < 1:
        return 1
    return 1 << (n - 1).bit_length()

def opt(gemm, arch):
    #1. Find Tile Size
    m_tile = 4 * arch.redmule_ce_height
    n_tile = 4 * arch.redmule_ce_height
    m_tile = gemm.m_size if gemm.m_size < m_tile else m_tile
    n_tile = gemm.n_size if gemm.n_size < n_tile else n_tile
    assert(gemm.m_size % m_tile == 0)
    assert(gemm.n_size % n_tile == 0)

    #2. Determine Group Scale
    scale_y = gemm.m_size // m_tile
    scale_x = gemm.n_size // n_tile
    try:
        is_power_of_two("scale_x", scale_x)
    except AssertionError as e:
        #Rescale to the next closet power of 2 number
        scale_x = next_power_of_2(scale_x)
        assert(gemm.n_size % scale_x == 0)
        n_tile = gemm.n_size // scale_x
    try:
        is_power_of_two("scale_y", scale_y)
    except AssertionError as e:
        #Rescale to the next closet power of 2 number
        scale_y = next_power_of_2(scale_y)
        assert(gemm.m_size % scale_y == 0)
        m_tile = gemm.m_size // scale_y
    scale_y = arch.num_cluster_y if scale_y > arch.num_cluster_y else scale_y
    scale_x = arch.num_cluster_x if scale_x > arch.num_cluster_x else scale_x


    #3. Determine Split K or Split N
    gemm_list = []
    group_number = 1
    group_reduce = 0
    group_splitk = 0
    group_splitn = 0
    group_gap_x = 0
    group_gap_w = 0
    group_gap_z = 0
    k_tile = 4 * arch.redmule_ce_height
    elem_size = 1 if gemm.dtype == 'fp8' else 2
    available_group = (arch.num_cluster_y * arch.num_cluster_x) // (scale_y * scale_x)

    ## First Discuss the Split K Option
    try_splitK = 2
    while try_splitK <= available_group:
        if gemm.k_size % try_splitK == 0:
            group_number = try_splitK
        else:
            break
            pass
        try_splitK *= 2
        pass
    gemm.strategy  = "Nosplit"
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
        group_gap_x = splitedK * elem_size
        group_gap_w = splitedK * gemm.n_size * elem_size
        group_gap_z = 0
        gemm.strategy  = "splitK"
        pass

    gemm.m_tile                  = m_tile
    gemm.n_tile                  = n_tile
    gemm.k_tile                  = k_tile
    gemm.summa_scale_x           = scale_x
    gemm.summa_scale_y           = scale_y
    gemm.summa_group_number      = group_number
    gemm.summa_group_reduce      = group_reduce
    gemm.summa_group_splitk      = group_splitk
    gemm.summa_group_splitn      = group_splitn
    gemm.summa_group_gap_x       = group_gap_x
    gemm.summa_group_gap_w       = group_gap_w
    gemm.summa_group_gap_z       = group_gap_z
    gemm_list.append(copy.deepcopy(gemm))

    #If we enconter the dilema to also consider Split N
    group_number = 1
    group_reduce = 0
    group_splitk = 0
    group_splitn = 0
    group_gap_x = 0
    group_gap_w = 0
    group_gap_z = 0
    k_tile = 4 * arch.redmule_ce_height
    if available_group > 1 and (gemm.n_size // (scale_x * n_tile)) > 1:
        if gemm.k_size < k_tile: k_tile = gemm.k_size
        group_number = gemm.n_size // (scale_x * n_tile)
        group_number = min(available_group, group_number)
        group_splitn = 1
        group_gap_w = scale_x * n_tile * elem_size
        group_gap_z = scale_x * n_tile * elem_size

        #Setup the gemm configuration
        gemm.m_tile                  = m_tile
        gemm.n_tile                  = n_tile
        gemm.k_tile                  = k_tile
        gemm.summa_scale_x           = scale_x
        gemm.summa_scale_y           = scale_y
        gemm.summa_group_number      = group_number
        gemm.summa_group_reduce      = group_reduce
        gemm.summa_group_splitk      = group_splitk
        gemm.summa_group_splitn      = group_splitn
        gemm.summa_group_gap_x       = group_gap_x
        gemm.summa_group_gap_w       = group_gap_w
        gemm.summa_group_gap_z       = group_gap_z
        gemm.strategy                = "splitN"
        gemm_list.append(copy.deepcopy(gemm))
        pass

    return gemm_list
    pass


def ofdp_opt(gemm, arch):
    #1. Find Tile Size
    m_tile = 4 * arch.redmule_ce_height
    n_tile = 4 * arch.redmule_ce_height
    m_tile = gemm.m_size if gemm.m_size < m_tile else m_tile
    n_tile = gemm.n_size if gemm.n_size < n_tile else n_tile
    assert(gemm.m_size % m_tile == 0)
    assert(gemm.n_size % n_tile == 0)

    #2. Determine Group Scale
    scale_y = gemm.m_size // m_tile
    scale_x = gemm.n_size // n_tile
    try:
        is_power_of_two("scale_x", scale_x)
    except AssertionError as e:
        #Rescale to the next closet power of 2 number
        scale_x = next_power_of_2(scale_x)
        assert(gemm.n_size % scale_x == 0)
        n_tile = gemm.n_size // scale_x
    try:
        is_power_of_two("scale_y", scale_y)
    except AssertionError as e:
        #Rescale to the next closet power of 2 number
        scale_y = next_power_of_2(scale_y)
        assert(gemm.m_size % scale_y == 0)
        m_tile = gemm.m_size // scale_y
    scale_y = arch.num_cluster_y if scale_y > arch.num_cluster_y else scale_y
    scale_x = arch.num_cluster_x if scale_x > arch.num_cluster_x else scale_x


    #3. Determine Split K
    elem_size = 1 if gemm.dtype == 'fp8' else 2
    group_number = (arch.num_cluster_y * arch.num_cluster_x) // (scale_y * scale_x)
    group_number = gemm.ofdp_splitk_num if gemm.ofdp_splitk_num < group_number else group_number
    k_splited_size = gemm.k_size // gemm.ofdp_splitk_num
    k_tile = 4 * arch.redmule_ce_height if k_splited_size > (4 * arch.redmule_ce_height) else k_splited_size
    repeat = (gemm.ofdp_splitk_num + group_number - 1) // group_number
    group_reduce = 0
    group_splitk = repeat
    group_splitn = 0
    group_gap_x = k_splited_size * elem_size
    group_gap_w = gemm.n_size * elem_size
    group_gap_z = k_splited_size * elem_size

    # Finalize Configuration
    gemm.m_tile                  = m_tile
    gemm.n_tile                  = n_tile
    gemm.k_tile                  = k_tile
    gemm.summa_scale_x           = scale_x
    gemm.summa_scale_y           = scale_y
    gemm.summa_group_number      = group_number
    gemm.summa_group_reduce      = group_reduce
    gemm.summa_group_splitk      = group_splitk
    gemm.summa_group_splitn      = group_splitn
    gemm.summa_group_gap_x       = group_gap_x
    gemm.summa_group_gap_w       = group_gap_w
    gemm.summa_group_gap_z       = group_gap_z

    return gemm, repeat
    pass
