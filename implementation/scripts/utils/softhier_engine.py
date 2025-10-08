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
import matplotlib.pyplot as plt
import utils.kernel_configuration as kc

sw_dict = {
    "gemm" : "SummaGEMM",
    "norm" : "RMSNorm",
    "attn" : "FlatAttention",
    "rope" : "RoPE",
    "acti" : "Activation"
}

class SoftHier(object):
    """docstring for SoftHier"""
    def __init__(self, softhier_root, kernel_root, output_root):
        super(SoftHier, self).__init__()
        self.softhier_root = Path(softhier_root).resolve()
        self.kernel_root = Path(kernel_root).resolve()
        self.output_root = Path(output_root).resolve()
        if not self.softhier_root.exists():
            raise RuntimeError(f"SoftHier Root Not Exist at : {softhier_root}")
            pass
        if not self.kernel_root.exists():
            raise RuntimeError(f"Kerenl Root Not Exist at : {kernel_root}")
            pass
        if not self.output_root.exists():
            raise RuntimeError(f"Output Root Not Exist at : {output_root}")
            pass
        self.output_folder = self.output_root / ("llm_" + datetime.now().strftime("%Y%m%d_%H%M%S"))
        os.system(f"mkdir -p {self.output_folder}")
        self.arch = None
        self.arch_path = None
        self.logout_option = " > "

    def launch_softhier(self, pld_path = None, target = "runv"):
        if pld_path is not None:
            pld = Path(pld_path).resolve()
            assert pld.exists()
            cmd = f"pld={pld} make -C {self.softhier_root} {target}"
        else:
            cmd = f"make -C {self.softhier_root} {target}"
            pass
        print(f"[System Call] {cmd}")
        pass

    def get_runtime_ns(self, trace_file):
        pattern = r"\[Performance Counter\]: Execution period is [0-9]+ ns"
        a = subprocess.run(["grep", "-E", pattern, trace_file], capture_output=True, text=True, check=False)
        line = a.stdout.splitlines()[0]
        num_pattern = re.compile(r"Execution period is (\d+) ns")
        match = num_pattern.search(line)
        if match:
            return int(match.group(1))
        else:
            raise RuntimeError(f"[SoftHier Result Error] performance is not exist in trace file : {trace_file}")
            pass
        pass

    def record_info(self, info_dict):
        #Record information
        for k, v in info_dict.items():
            file_name  = self.output_folder_info / f"{k}.yaml"
            with open(file_name, "w") as f:
                yaml.dump(v, f)
            pass
        pass

    def compile_hw(self, arch, arch_path):
        if not Path(arch_path).exists():
            raise RuntimeError(f"Arch Path Not Exist at : {arch_path}")
            pass
        self.arch_path = Path(arch_path).resolve()
        self.arch = arch
        self.output_folder_hwcfg = self.output_folder / "softhier_config"
        os.system(f"mkdir -p {self.output_folder_hwcfg}")
        os.system(f"cp {self.arch_path} {self.output_folder_hwcfg}")
        cmd = f"cfg={self.arch_path} make -C {self.softhier_root} hw > {self.output_folder_hwcfg}/softhier_hw.log 2>&1"
        print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0
        pass

    def register_workload(self, name, info_dict):
        folder_name = re.sub(r'[^\w.\-]', '_', name)
        self.output_folder_log = self.output_folder / folder_name / "log"
        self.output_folder_trace = self.output_folder / folder_name / "trace"
        self.output_folder_info = self.output_folder / folder_name / "info"
        os.system(f"mkdir -p {self.output_folder_log}")
        os.system(f"mkdir -p {self.output_folder_trace}")
        os.system(f"mkdir -p {self.output_folder_info}")
        self.record_info(info_dict)
        pass

    def gemm_auto(self, cfg, data, name):
        """
        Automatic Hyper-parameteric GEMM
        """
        # Check Configuration and Automatic Tiling/Scheduling
        app_path = self.kernel_root / "SummaGEMM"

        # [TODO] need to change this naive check and do automatic parameterization
        assert cfg.summa_scale_x == self.arch.num_cluster_x
        assert cfg.summa_scale_y == self.arch.num_cluster_y
        assert cfg.m_size > (cfg.summa_scale_y * cfg.m_tile)
        assert cfg.n_size > (cfg.summa_scale_x * cfg.n_tile)
        assert cfg.k_size > cfg.k_tile
        assert cfg.summa_group_reduce == 0
        assert cfg.summa_group_gap_x  == 0
        assert cfg.summa_group_gap_w  == 0
        assert cfg.summa_group_gap_z  == 0
        # time.sleep(1); return {"runtime" : 100}

        # Generate Configuration
        gemm_cfg_h = self.kernel_root / "SummaGEMM" / "include" / "gemm.h"
        appendix = []
        if cfg.resha_x_from_enable:
            resha_x_m_size = cfg.resha_x_from_m
            resha_x_k_size = (cfg.m_size * cfg.k_size) // cfg.resha_x_from_m
            assert cfg.m_size % cfg.m_tile == 0, f"X Reshaped Enabled, We must make sure M size and tile are aligned"
            assert cfg.k_size % cfg.k_tile == 0, f"X Reshaped Enabled, We must make sure K size and tile are aligned"
            assert resha_x_k_size > 0, f"Invalide X Reshape {resha_x_m_size}, {resha_x_k_size}"
            assert resha_x_m_size >= cfg.m_tile, f"X Reshape Cross M Tiles"
            assert resha_x_k_size >= cfg.k_tile, f"X Reshape Cross K Tiles"
            assert resha_x_m_size % cfg.m_tile == 0, f"X Reshaped Enabled, We must make sure reshaped M size and tile are aligned"
            assert resha_x_k_size % cfg.k_tile == 0, f"X Reshaped Enabled, We must make sure reshaped K size and tile are aligned"
            if cfg.resha_x_from_m > cfg.m_size:
                # Tall
                appendix.append(f"#define GEMM_RESHAPE_X_FROM_K ((uint64_t){resha_x_k_size})")
                appendix.append(f"#define GEMM_RESHA_X_FROM_TALL")
            elif cfg.resha_x_from_m < cfg.m_size:
                # Thin
                appendix.append(f"#define GEMM_RESHAPE_X_FROM_K ((uint64_t){resha_x_k_size})")
                appendix.append(f"#define GEMM_RESHA_X_FROM_THIN")
            else:
                raise RuntimeError(f"X Reshaped Enabled But Dimension Not Changed")
                pass
            pass

        if cfg.resha_z_to_enable:
            resha_z_m_size = cfg.resha_z_to_m
            resha_z_n_size = (cfg.m_size * cfg.n_size) // cfg.resha_z_to_m
            assert cfg.m_size % cfg.m_tile == 0, f"Z Reshaped Enabled, We must make sure M size and tile are aligned"
            assert cfg.n_size % cfg.n_tile == 0, f"Z Reshaped Enabled, We must make sure N size and tile are aligned"
            assert resha_z_n_size > 0, f"Invalide Z Reshape {resha_z_m_size}, {resha_z_n_size}"
            assert resha_z_m_size >= cfg.m_tile, f"Z Reshape Cross M Tiles"
            assert resha_z_n_size >= cfg.n_tile, f"Z Reshape Cross N Tiles"
            assert resha_z_m_size % cfg.m_tile == 0, f"Z Reshaped Enabled, We must make sure reshaped M size and tile are aligned"
            assert resha_z_n_size % cfg.n_tile == 0, f"Z Reshaped Enabled, We must make sure reshaped N size and tile are aligned"
            if cfg.resha_z_to_m > cfg.m_size:
                # Tall
                appendix.append(f"#define GEMM_RESHAPE_Z_TO_N ((uint64_t){resha_z_n_size})")
                appendix.append(f"#define GEMM_RESHA_Z_TO_TALL")
            elif cfg.resha_z_to_m < cfg.m_size:
                # Thin
                appendix.append(f"#define GEMM_RESHAPE_Z_TO_N ((uint64_t){resha_z_n_size})")
                appendix.append(f"#define GEMM_RESHA_Z_TO_THIN")
            else:
                raise RuntimeError(f"Z Reshaped Enabled But Dimension Not Changed")
                pass
            pass
        kc.generate_config_C_header("GEMM", cfg, gemm_cfg_h, cfg.dtype, cfg.summa_numer, appendix=appendix)

        # Generate Preload C Header File
        gemm_pld_h = self.kernel_root / "SummaGEMM" / "include" / "preload.h"
        X_addr = data["input"]["addr"]
        W_addr = data["weight"]["addr"]
        Z_eaddr = data["output"]["addr"]
        Z_gaddr = Z_eaddr
        with open(gemm_pld_h, 'w') as file:
            file.write('#ifndef _GEMM_PRELOAD_H_\n')
            file.write('#define _GEMM_PRELOAD_H_\n\n')
            file.write(f'#define {"X_addr".upper()} ((uint64_t){X_addr: #x})\n')
            file.write(f'#define {"Z_eaddr".upper()} ((uint64_t){Z_eaddr: #x})\n')
            file.write(f'#define {"W_addr".upper()} ((uint64_t){W_addr: #x})\n')
            file.write(f'#define {"Z_gaddr".upper()} ((uint64_t){Z_gaddr: #x})\n')
            file.write('\n#endif // _GEMM_PRELOAD_H_\n')
            file.close()

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Execute SoftHier Simulation
        cmd = f"make -C {self.softhier_root} runv > {self.output_folder_trace}/{name}.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Anaylze Result
        result = {}
        result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
        return result
        pass

    def norm(self, cfg, data, name):
        """
        Normalizaiton
        """
        # Check Configuration
        app_path = self.kernel_root / "RMSNorm"
        # time.sleep(1); return {"runtime" : 100}
        # Generate Configuration
        norm_cfg_h = self.kernel_root / "RMSNorm" / "include" / "norm.h"
        kc.generate_config_C_header("NORM", cfg, norm_cfg_h, cfg.dtype, cfg.norm_numer)

        # Generate Preload C Header File
        norm_pld_h = self.kernel_root / "RMSNorm" / "include" / "preload.h"
        I_addr = data["input"]["addr"]
        O_eaddr = data["output"]["addr"]
        O_gaddr = O_eaddr
        with open(norm_pld_h, 'w') as file:
            file.write('#ifndef _NORM_PRELOAD_H_\n')
            file.write('#define _NORM_PRELOAD_H_\n\n')
            file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
            file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
            file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
            file.write('\n#endif // _NORM_PRELOAD_H_\n')
            file.close()

        # Generate Preload Data

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Execute SoftHier Simulation
        cmd = f"make -C {self.softhier_root} runv > {self.output_folder_trace}/{name}.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Anaylze Result
        result = {}
        result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
        return result
        pass

    def rope(self, cfg, data, name):
        """
        RoPE
        """
        # Check Configuration
        app_path = self.kernel_root / "RoPE"
        assert cfg.m_size % cfg.contiguous_length == 0
        # time.sleep(1); return {"runtime" : 100}

        # Generate Configuration
        rope_cfg_h = self.kernel_root / "RoPE" / "include" / "rope.h"
        kc.generate_config_C_header("ROPE", cfg, rope_cfg_h, cfg.dtype, cfg.rope_numer)

        # Generate Preload C Header File
        rope_pld_h = self.kernel_root / "RoPE" / "include" / "preload.h"
        I_addr = data["input"]["addr"]
        C_addr = data["cos"]["addr"]
        S_addr = data["sin"]["addr"]
        P_addr = data["position"]["addr"]
        O_eaddr = data["output"]["addr"]
        O_gaddr = O_eaddr
        with open(rope_pld_h, 'w') as file:
            file.write('#ifndef _ROPE_PRELOAD_H_\n')
            file.write('#define _ROPE_PRELOAD_H_\n\n')
            file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
            file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
            file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
            file.write(f'#define {"C_addr".upper()} ((uint64_t){C_addr: #x})\n')
            file.write(f'#define {"S_addr".upper()} ((uint64_t){S_addr: #x})\n')
            file.write(f'#define {"P_addr".upper()} ((uint64_t){P_addr: #x})\n')
            file.write('\n#endif // _ROPE_PRELOAD_H_\n')
            file.close()

        # Generate Preload Data

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Execute SoftHier Simulation
        cmd = f"make -C {self.softhier_root} runv > {self.output_folder_trace}/{name}.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Anaylze Result
        result = {}
        result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
        return result
        pass

    def acti(self, cfg, data, name):
        """
        Activation
        """
        # Check Configuration
        app_path = self.kernel_root / "Activation"
        algo_list = ['sigmoid', 'relu', 'silu']
        assert cfg.algo in algo_list
        # time.sleep(1); return {"runtime" : 100}

        # Generate Configuration
        acti_cfg_h = self.kernel_root / "Activation" / "include" / "acti.h"
        kc.generate_config_C_header("ACTI", cfg, acti_cfg_h, cfg.dtype, cfg.acti_numer)

        # Generate Preload C Header File
        acti_pld_h = self.kernel_root / "Activation" / "include" / "preload.h"
        I_addr = data["input"]["addr"]
        G_addr = I_addr if "gate" not in data else data["gate"]["addr"]
        B_addr = I_addr if "bias" not in data else data["bias"]["addr"]
        O_eaddr = data["output"]["addr"]
        O_gaddr = O_eaddr
        with open(acti_pld_h, 'w') as file:
            file.write('#ifndef _ACTI_PRELOAD_H_\n')
            file.write('#define _ACTI_PRELOAD_H_\n\n')
            file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
            file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
            file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
            file.write(f'#define {"G_addr".upper()} ((uint64_t){G_addr: #x})\n')
            file.write(f'#define {"B_addr".upper()} ((uint64_t){B_addr: #x})\n')
            file.write('\n#endif // _ACTI_PRELOAD_H_\n')
            file.close()

        # Generate Preload Data

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Execute SoftHier Simulation
        cmd = f"make -C {self.softhier_root} runv > {self.output_folder_trace}/{name}.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Anaylze Result
        result = {}
        result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
        return result
        pass

    def addi(self, cfg, data, name):
        """
        Addition
        """
        # Check Configuration
        app_path = self.kernel_root / "Activation"
        assert cfg.algo == 'none'
        assert cfg.gate_enable == 0
        assert cfg.bias_enable == 1
        # time.sleep(1); return {"runtime" : 100}

        # Generate Configuration
        acti_cfg_h = self.kernel_root / "Activation" / "include" / "acti.h"
        kc.generate_config_C_header("ACTI", cfg, acti_cfg_h, cfg.dtype, cfg.acti_numer)

        # Generate Preload C Header File
        acti_pld_h = self.kernel_root / "Activation" / "include" / "preload.h"
        I_addr = data["input"]["addr"]
        G_addr = I_addr if "gate" not in data else data["gate"]["addr"]
        B_addr = I_addr if "bias" not in data else data["bias"]["addr"]
        O_eaddr = data["output"]["addr"]
        O_gaddr = O_eaddr
        with open(acti_pld_h, 'w') as file:
            file.write('#ifndef _ACTI_PRELOAD_H_\n')
            file.write('#define _ACTI_PRELOAD_H_\n\n')
            file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
            file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
            file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
            file.write(f'#define {"G_addr".upper()} ((uint64_t){G_addr: #x})\n')
            file.write(f'#define {"B_addr".upper()} ((uint64_t){B_addr: #x})\n')
            file.write('\n#endif // _ACTI_PRELOAD_H_\n')
            file.close()

        # Generate Preload Data

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Execute SoftHier Simulation
        cmd = f"make -C {self.softhier_root} runv > {self.output_folder_trace}/{name}.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Anaylze Result
        result = {}
        result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
        return result
        pass

    def flat_attn_auto(self, cfg, data, name):
        """
        Automatic Hyper-parameteric FlatAttention
        """
        # Check Configuration
        app_path = self.kernel_root / "FlatAttention"

        # [TODO] need to change this naive check and do automatic parameterization
        assert self.arch.num_cluster_x % cfg.flatten_scale_x == 0
        assert self.arch.num_cluster_y % cfg.flatten_scale_y == 0
        assert (cfg.batch_size * cfg.num_head_group) % ((self.arch.num_cluster_x // cfg.flatten_scale_x) * (self.arch.num_cluster_y // cfg.flatten_scale_y)) == 0
        if cfg.flatten_async == 1:
            assert (cfg.batch_size * cfg.num_head_group) % (2 * (self.arch.num_cluster_x // cfg.flatten_scale_x) * (self.arch.num_cluster_y // cfg.flatten_scale_y)) == 0
            pass
        assert cfg.kv_sequence_length % cfg.flatten_shape_x == 0
        assert (cfg.q_sequence_length * cfg.speculative_length * (cfg.num_head // cfg.num_head_group)) % cfg.flatten_shape_x == 0
        # time.sleep(1); return {"runtime" : 100}

        # Generate Configuration
        attn_cfg_h = self.kernel_root / "FlatAttention" / "include" / "attn.h"
        kc.generate_config_C_header("ATTN", cfg, attn_cfg_h, cfg.dtype, cfg.flatten_numer)

        # Generate Preload C Header File
        attn_pld_h = self.kernel_root / "FlatAttention" / "include" / "preload.h"
        Q_addr = data["q"]["addr"]
        K_addr = data["k"]["addr"]
        V_addr = data["v"]["addr"]
        O_eaddr = data["o"]["addr"]
        O_gaddr = O_eaddr
        with open(attn_pld_h, 'w') as file:
            file.write('#ifndef _ATTN_PRELOAD_H_\n')
            file.write('#define _ATTN_PRELOAD_H_\n\n')
            file.write(f'#define {"Q_addr".upper()} ((uint64_t){Q_addr: #x})\n')
            file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
            file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
            file.write(f'#define {"K_addr".upper()} ((uint64_t){K_addr: #x})\n')
            file.write(f'#define {"V_addr".upper()} ((uint64_t){V_addr: #x})\n')
            file.write('\n#endif // _ATTN_PRELOAD_H_\n')
            file.close()

        # [TODO] Generate Preload Data

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Execute SoftHier Simulation
        cmd = f"make -C {self.softhier_root} runv > {self.output_folder_trace}/{name}.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        # Anaylze Result
        result = {}
        result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
        return result
        pass
        