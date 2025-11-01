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
import time
import yaml
import torch
import shutil
import argparse
import subprocess
import numpy as np
import importlib.util
from tqdm import tqdm
import preload as pld
from rich import print
from pathlib import Path
from datetime import datetime
import torch.nn.functional as F

class C2CEngine(object):
	"""docstring for C2CEngine"""
	def __init__(self, softhier_root, output_root, num_chips):
		super(C2CEngine, self).__init__()
		self.softhier_root = Path(softhier_root).resolve()
        self.output_root = Path(output_root).resolve()
        if not self.softhier_root.exists():
            raise RuntimeError(f"SoftHier Root Not Exist at : {softhier_root}")
            pass
        if not self.output_root.exists():
            raise RuntimeError(f"Output Root Not Exist at : {output_root}")
            pass
		