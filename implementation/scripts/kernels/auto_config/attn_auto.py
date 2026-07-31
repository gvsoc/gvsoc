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

def next_power_of_2(n: int) -> int:
    if n < 1:
        return 1
    return 1 << (n - 1).bit_length()

def opt(attn, arch):
    seqlen_y = attn.q_sequence_length * attn.speculative_length * attn.num_head // attn.num_head_group
    seqlen_x = attn.kv_sequence_length

    #1. Find Tile Size
    y_tile = 4 * arch.redmule_ce_height
    x_tile = 4 * arch.redmule_ce_height
    y_tile = seqlen_y if seqlen_y < y_tile else y_tile
    x_tile = seqlen_x if seqlen_x < x_tile else x_tile
    assert(seqlen_x % x_tile == 0)
    assert(seqlen_y % y_tile == 0)

    if hasattr(attn, 'use_flash_attn') and (attn.use_flash_attn == True):
        attn.flatten_scale_x         = 1
        attn.flatten_scale_y         = 1
        attn.flatten_shape_x         = x_tile
        attn.flatten_shape_y         = y_tile
        attn.flatten_async           = 0
        return attn
        pass

    y_tile = y_tile if (4 * arch.redmule_ce_height > 64) else 32
    x_tile = x_tile if (4 * arch.redmule_ce_height > 64) else 32

    #2. Determine Group Scale
    scale_y = seqlen_y // y_tile
    scale_x = seqlen_x // x_tile
    try:
        is_power_of_two("scale_x", scale_x)
    except AssertionError as e:
        #Rescale to the next closet power of 2 number
        scale_x = next_power_of_2(scale_x)
        assert(seqlen_x % scale_x == 0)
        x_tile = seqlen_x // scale_x
    try:
        is_power_of_two("scale_y", scale_y)
    except AssertionError as e:
        #Rescale to the next closet power of 2 number
        scale_y = next_power_of_2(scale_y)
        assert(seqlen_y % scale_y == 0)
        y_tile = seqlen_y // scale_y
    scale_y = arch.num_cluster_y if scale_y > arch.num_cluster_y else scale_y
    scale_x = arch.num_cluster_x if scale_x > arch.num_cluster_x else scale_x

    #3. Determine Async Option
    available_group = (arch.num_cluster_y * arch.num_cluster_x) // (scale_y * scale_x)
    if attn.batch_size <= available_group:
        flatten_async = 0
    else:
        assert(attn.batch_size % available_group == 0)
        flatten_async = 1
        pass

    attn.flatten_scale_x         = scale_x
    attn.flatten_scale_y         = scale_y
    attn.flatten_shape_x         = scale_x * x_tile
    attn.flatten_shape_y         = scale_y * y_tile
    attn.flatten_async           = flatten_async
    return attn
    pass