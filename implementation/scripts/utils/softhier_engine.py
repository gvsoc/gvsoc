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
import preload as pld
from rich import print
from pathlib import Path
import torch.nn.functional as F
import matplotlib.pyplot as plt

sw_dict = {
    "gemm" : "SummaGEMM",
    "norm" : "RMSNorm",
    "attn" : "FlatAttention",
    "rope" : "RoPE",
    "acti" : "Activation"
}

class SoftHier(object):
    """docstring for SoftHier"""
    def __init__(self, softhier_root, kernel_root):
        super(SoftHier, self).__init__()
        self.softhier_root = Path(softhier_root).resolve()
        self.kernel_root = Path(kernel_root).resolve()
        if not self.softhier_root.exists():
            raise RuntimeError(f"SoftHier Root Not Exist at : {softhier_root}")
            pass
        if not self.kernel_root.exists():
            raise RuntimeError(f"Kerenl Root Not Exist at : {kernel_root}")
            pass
        self.arch = None
        self.arch_path = None

    def lanuch_softhier(pld_path):
        pass

    def compile_hw(arch, arch_path):
        if not Path(arch_path).exists():
            raise RuntimeError(f"Arch Path Not Exist at : {arch_path}")
            pass
        self.arch_path = Path(arch_path).resolve()
        self.arch = arch
        pass

    def gemm(cfg, a, b, c, a_addr, b_addr, c_addr, bias = None, bias_addr = None, number_check = 0):
        cfg.m_size = a.shape[0]
        cfg.n_size = b.shape[1]
        cfg.k_size = a.shape[1]
        pass
        