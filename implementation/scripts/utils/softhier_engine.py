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
import kernels.auto_config.gemm_auto as gemm_optimizer
import kernels.auto_config.tmla_auto as tmla_optimizer
import kernels.auto_config.attn_auto as attn_optimizer
import kernels.auto_config.moex_auto as moex_optimizer

sw_dict = {
    "gemm" : "SummaGEMM",
    "norm" : "RMSNorm",
    "attn" : "FlatAttention",
    "rope" : "RoPE",
    "acti" : "Activation"
}

class SoftHier(object):
    """docstring for SoftHier"""
    def __init__(self, softhier_root, kernel_root, output_root, tag=None):
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
        prefix = "llm_" if tag == None else f"{tag}_"
        output_name = prefix + datetime.now().strftime("%Y%m%d_%H%M%S")
        self.output_folder = self.output_root / output_name
        os.system(f"mkdir -p {self.output_folder}")
        self.arch = None
        self.arch_path = None
        self.run_option = "runq"

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

    def record_info(self, info_dict, subdir = None):
        #Record information
        prefix = self.output_folder_info
        if subdir != None:
            prefix = self.output_folder_info / subdir
            os.system(f"mkdir -p {prefix}")
            pass
        for k, v in info_dict.items():
            file_name  = prefix / f"{k}.yaml"
            with open(file_name, "w") as f:
                yaml.dump(v, f, sort_keys=False)
            pass
        pass

    def compile_hw(self, arch, arch_path):
        if not Path(arch_path).exists():
            raise RuntimeError(f"Arch Path Not Exist at : {arch_path}")
            pass
        self.arch_path = Path(arch_path).resolve()
        self.arch = arch
        self.arch.cycles_per_ns = 1 if not hasattr(arch, 'frequence') else (arch.frequence / 1000000000)
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

    def gemm_auto(self, cfg, data, name, dry_run = False):
        """
        Automatic Hyper-parameteric GEMM
        """
        # Check Configuration and Automatic Tiling/Scheduling
        app_path = self.kernel_root / "SummaGEMM"
        best_log = None
        result = {}
        gemm_flop = 2 * cfg.m_size * cfg.n_size * cfg.k_size
        result['FLOP'] = gemm_flop

        # Naive check and do automatic parameterization
        cfg_list = gemm_optimizer.opt(cfg, self.arch)
        for cfg in cfg_list:
            self.record_info({f"{name}.{cfg.strategy}" : cfg}, subdir="kernels")

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
            cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}.{cfg.strategy}_sw.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            if not dry_run:
                # Execute SoftHier Simulation
                cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.{cfg.strategy}.log 2>&1"
                # print(f"[System Call] {cmd}")
                assert os.system(cmd) == 0

                # Select Log
                test_log = f"{self.output_folder_trace}/{name}.{cfg.strategy}.log"
                if best_log == None:
                    best_log = test_log
                else:
                    best_log = test_log if (self.get_runtime_ns(best_log) > self.get_runtime_ns(test_log)) else best_log
                    pass

                # Anaylze Result
                runtime = self.get_runtime_ns(best_log)
                cycles = runtime * self.arch.cycles_per_ns
                peak_flop_per_cycle = 2 * self.arch.num_cluster_x * self.arch.num_cluster_y * self.arch.redmule_ce_height * self.arch.redmule_ce_width
                achieved_flop_per_cycle = gemm_flop / cycles
                redmule_uti = achieved_flop_per_cycle / peak_flop_per_cycle
                elem_size = 1 if cfg.dtype == 'fp8' else 2
                arithmetic_intensity = gemm_flop / (elem_size * (cfg.m_size * cfg.k_size + cfg.n_size * cfg.k_size + cfg.m_size * cfg.n_size))
                result["runtime"] = runtime
                result["peak_flop_per_cycle"] = peak_flop_per_cycle
                result["achieved_flop_per_cycle"] = achieved_flop_per_cycle
                result["redmule_uti"] = redmule_uti
                result["arithmetic_intensity"] = arithmetic_intensity
                pass
            pass
        return result
        pass

    def norm(self, cfg, data, name, dry_run = False):
        """
        Normalizaiton
        """
        # Check Configuration
        app_path = self.kernel_root / "RMSNorm"
        self.record_info({name : cfg}, subdir="kernels")

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

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            result = {}
            result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            result["redmule_uti"] = 0
            return result
        pass

    def rope(self, cfg, data, name, dry_run = False):
        """
        RoPE
        """
        # Check Configuration
        app_path = self.kernel_root / "RoPE"
        assert cfg.m_size % cfg.contiguous_length == 0
        self.record_info({name : cfg}, subdir="kernels")

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

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            result = {}
            result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            result["redmule_uti"] = 0
            return result
        pass

    def acti(self, cfg, data, name, dry_run = False):
        """
        Activation
        """
        # Check Configuration
        app_path = self.kernel_root / "Activation"
        algo_list = ['sigmoid', 'relu', 'silu']
        assert cfg.algo in algo_list
        self.record_info({name : cfg}, subdir="kernels")

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

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            result = {}
            result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            result["redmule_uti"] = 0
            return result
        pass

    def addi(self, cfg, data, name, dry_run = False):
        """
        Addition
        """
        # Check Configuration
        app_path = self.kernel_root / "Activation"
        assert cfg.algo == 'none'
        assert cfg.gate_enable == 0
        assert cfg.bias_enable == 1
        self.record_info({name : cfg}, subdir="kernels")

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

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            result = {}
            result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            result["redmule_uti"] = 0
            return result
        pass

    def flat_attn_auto(self, cfg, data, name, dry_run = False):
        """
        Automatic Hyper-parameteric FlatAttention
        """
        # Check Configuration
        app_path = self.kernel_root / "FlatAttention"

        # Naive check and do automatic parameterization
        cfg = attn_optimizer.opt(cfg, self.arch)
        self.record_info({name : cfg}, subdir="kernels")
        result = {}
        flat_attn_flop = 4 * cfg.q_sequence_length * cfg.speculative_length * cfg.kv_sequence_length * cfg.head_dimemsion * cfg.num_head * cfg.batch_size
        result['FLOP'] = flat_attn_flop

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

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            runtime = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            cycles = runtime * self.arch.cycles_per_ns
            peak_flop_per_cycle = 2 * self.arch.num_cluster_x * self.arch.num_cluster_y * self.arch.redmule_ce_height * self.arch.redmule_ce_width
            achieved_flop_per_cycle = flat_attn_flop / cycles
            redmule_uti = achieved_flop_per_cycle / peak_flop_per_cycle
            elem_size = 1 if cfg.dtype == 'fp8' else 2
            arithmetic_intensity = flat_attn_flop / (elem_size * cfg.batch_size * (2 * cfg.num_head * cfg.q_sequence_length * cfg.speculative_length * cfg.head_dimemsion + 2 * cfg.num_head_group * cfg.kv_sequence_length * cfg.head_dimemsion))
            result["runtime"] = runtime
            result["peak_flop_per_cycle"] = peak_flop_per_cycle
            result["achieved_flop_per_cycle"] = achieved_flop_per_cycle
            result["redmule_uti"] = redmule_uti
            result["arithmetic_intensity"] = arithmetic_intensity
            pass
        return result
        pass

    def flat_mla_auto(self, cfg, data, name, dry_run = False):
        """
        Automatic Hyper-parameteric FlatMLA
        """
        # Check Configuration
        app_path = self.kernel_root / "FlatMLA"

        # Naive check and do automatic parameterization
        cfg = tmla_optimizer.opt(cfg, self.arch)
        self.record_info({name : cfg}, subdir="kernels")
        result = {}
        seqlen_q = cfg.q_sequence_length * cfg.speculative_length * cfg.num_head
        seqlen_c = cfg.kv_sequence_length
        flat_mla_flop = 2 * cfg.batch_size * (seqlen_q * seqlen_c * (cfg.nope_head_dim + cfg.rope_head_dim) + seqlen_q * seqlen_c * cfg.nope_head_dim)
        result['FLOP'] = flat_mla_flop

        # Generate Configuration
        tmla_cfg_h = self.kernel_root / "FlatMLA" / "include" / "tmla.h"
        kc.generate_config_C_header("TMLA", cfg, tmla_cfg_h, cfg.dtype, cfg.flatten_numer)

        # Generate Preload C Header File
        tmla_pld_h = self.kernel_root / "FlatMLA" / "include" / "preload.h"
        QN_addr = data["qn"]["addr"]
        QR_addr = data["qr"]["addr"]
        O_eaddr = data["o"]["addr"]
        O_gaddr = O_eaddr
        CN_addr = data["cn"]["addr"]
        CR_addr = data["cr"]["addr"]
        with open(tmla_pld_h, 'w') as file:
            file.write('#ifndef _TMLA_PRELOAD_H_\n')
            file.write('#define _TMLA_PRELOAD_H_\n\n')
            file.write(f'#define {"QN_addr".upper()} ((uint64_t){QN_addr: #x})\n')
            file.write(f'#define {"QR_addr".upper()} ((uint64_t){QR_addr: #x})\n')
            file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
            file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
            file.write(f'#define {"CN_addr".upper()} ((uint64_t){CN_addr: #x})\n')
            file.write(f'#define {"CR_addr".upper()} ((uint64_t){CR_addr: #x})\n')
            file.write('\n#endif // _TMLA_PRELOAD_H_\n')
            file.close()

        # [TODO] Generate Preload Data

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            runtime = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            cycles = runtime * self.arch.cycles_per_ns
            peak_flop_per_cycle = 2 * self.arch.num_cluster_x * self.arch.num_cluster_y * self.arch.redmule_ce_height * self.arch.redmule_ce_width
            achieved_flop_per_cycle = flat_mla_flop / cycles
            redmule_uti = achieved_flop_per_cycle / peak_flop_per_cycle
            elem_size = 1 if cfg.dtype == 'fp8' else 2
            arithmetic_intensity = flat_mla_flop / (elem_size * cfg.batch_size * ((seqlen_q + seqlen_c) * (cfg.nope_head_dim + cfg.rope_head_dim) + seqlen_c * cfg.nope_head_dim + seqlen_q * cfg.nope_head_dim))
            result["runtime"] = runtime
            result["peak_flop_per_cycle"] = peak_flop_per_cycle
            result["achieved_flop_per_cycle"] = achieved_flop_per_cycle
            result["redmule_uti"] = redmule_uti
            result["arithmetic_intensity"] = arithmetic_intensity
            pass
        return result
        pass

    def moe_gate_topk(self, cfg, data, name, dry_run = False):
        """
        MoE Gate TopK
        """
        # Check Configuration
        app_path = self.kernel_root / "MoEGate"
        cfg = moex_optimizer.opt(cfg, self.arch)
        self.record_info({name : cfg}, subdir="kernels")

        # Generate Configuration
        moeg_cfg_h = self.kernel_root / "MoEGate" / "include" / "moeg.h"
        kc.generate_config_C_header("MOEG", cfg, moeg_cfg_h, cfg.dtype, cfg.moeg_numer)

        # Generate Preload C Header File
        moeg_pld_h = self.kernel_root / "MoEGate" / "include" / "preload.h"
        I_addr  = data['input']['addr']
        V_eaddr = data['output_val']['addr']
        V_gaddr = V_eaddr
        D_eaddr = data['output_idx']['addr']
        D_gaddr = D_eaddr
        with open(moeg_pld_h, 'w') as file:
            file.write('#ifndef _MOEG_PRELOAD_H_\n')
            file.write('#define _MOEG_PRELOAD_H_\n\n')
            file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
            file.write(f'#define {"V_eaddr".upper()} ((uint64_t){V_eaddr: #x})\n')
            file.write(f'#define {"V_gaddr".upper()} ((uint64_t){V_gaddr: #x})\n')
            file.write(f'#define {"D_eaddr".upper()} ((uint64_t){D_eaddr: #x})\n')
            file.write(f'#define {"D_gaddr".upper()} ((uint64_t){D_gaddr: #x})\n')
            file.write('\n#endif // _MOEG_PRELOAD_H_\n')
            file.close()

        # Generate Preload Data

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            result = {}
            result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            result["redmule_uti"] = 0
            return result
        pass

    def moe_dispatch(self, cfg, data, name, dry_run = False):
        """
        MoE Token Dispatch
        """
        # Check Configuration
        app_path = self.kernel_root / "MoEDispatch"
        cfg = moex_optimizer.opt(cfg, self.arch)
        self.record_info({name : cfg}, subdir="kernels")

        # Generate Configuration
        moed_cfg_h = self.kernel_root / "MoEDispatch" / "include" / "moed.h"
        kc.generate_config_C_header("MOED", cfg, moed_cfg_h, cfg.dtype, cfg.moed_numer)

        # Generate Preload C Header File
        moed_pld_h = self.kernel_root / "MoEDispatch" / "include" / "preload.h"
        I_addr = data['input']['addr']
        S_addr = data['merged_output']['addr']
        D_addr = data['input_idx']['addr']
        P_addr = data['output_pos']['addr']
        with open(moed_pld_h, 'w') as file:
            file.write('#ifndef _MOED_PRELOAD_H_\n')
            file.write('#define _MOED_PRELOAD_H_\n\n')
            file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
            file.write(f'#define {"S_addr".upper()} ((uint64_t){S_addr: #x})\n')
            file.write(f'#define {"D_addr".upper()} ((uint64_t){D_addr: #x})\n')
            file.write(f'#define {"P_addr".upper()} ((uint64_t){P_addr: #x})\n')
            file.write('\n#endif // _MOED_PRELOAD_H_\n')
            file.close()

        # Generate Preload Data
        moed_pld_elf = self.kernel_root / "MoEDispatch" / "preload.elf"
        assert(data['input_idx']['tensor'] != None)
        D_np = data['input_idx']['tensor'].to(torch.int32).cpu().numpy()
        pld.make_preload_elf(moed_pld_elf, [D_np], [D_addr])

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"pld={moed_pld_elf} make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            result = {}
            result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            result["redmule_uti"] = 0
            return result
        pass

    def moe_combine(self, cfg, data, name, dry_run = False):
        """
        MoE Token Combine
        """
        # Check Configuration
        app_path = self.kernel_root / "MoECombine"
        cfg = moex_optimizer.opt(cfg, self.arch)
        self.record_info({name : cfg}, subdir="kernels")

        # Generate Configuration
        moec_cfg_h = self.kernel_root / "MoECombine" / "include" / "moec.h"
        kc.generate_config_C_header("MOEC", cfg, moec_cfg_h, cfg.dtype, cfg.moec_numer)

        # Generate Preload C Header File
        moec_pld_h = self.kernel_root / "MoECombine" / "include" / "preload.h"
        I_addr  = data['merged_input']['addr']
        V_addr  = data['input_val']['addr']
        D_addr  = data['input_idx']['addr']
        P_addr  = data['input_pos']['addr']
        O_eaddr = data['output']['addr']
        O_gaddr = O_eaddr
        with open(moec_pld_h, 'w') as file:
            file.write('#ifndef _MOEC_PRELOAD_H_\n')
            file.write('#define _MOEC_PRELOAD_H_\n\n')
            file.write(f'#define {"I_addr".upper()} ((uint64_t){I_addr: #x})\n')
            file.write(f'#define {"V_addr".upper()} ((uint64_t){V_addr: #x})\n')
            file.write(f'#define {"D_addr".upper()} ((uint64_t){D_addr: #x})\n')
            file.write(f'#define {"P_addr".upper()} ((uint64_t){P_addr: #x})\n')
            file.write(f'#define {"O_eaddr".upper()} ((uint64_t){O_eaddr: #x})\n')
            file.write(f'#define {"O_gaddr".upper()} ((uint64_t){O_gaddr: #x})\n')
            file.write('\n#endif // _MOEC_PRELOAD_H_\n')
            file.close()

        # Generate Preload Data
        moec_pld_elf = self.kernel_root / "MoECombine" / "preload.elf"
        assert(data['input_idx']['tensor'] != None)
        assert(data['input_pos']['tensor'] != None)
        D_np = data['input_idx']['tensor'].to(torch.int32).cpu().numpy()
        P_np = data['input_pos']['tensor'].to(torch.int32).cpu().numpy()
        pld.make_preload_elf(moec_pld_elf, [D_np, P_np], [D_addr, P_addr])

        # Compile SW
        cmd = f"cfg={self.arch_path} app={app_path} make -C {self.softhier_root} sw > {self.output_folder_log}/kernel_{name}_sw.log 2>&1"
        # print(f"[System Call] {cmd}")
        assert os.system(cmd) == 0

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"pld={moec_pld_elf} make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            result = {}
            result["runtime"] = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log")
            result["redmule_uti"] = 0
            return result
        pass

    def split_concat(self, cfg, data, name, dry_run = False):
        #[TODO] Implementation
        if not dry_run:
            result = {"runtime" : 0}
            result["redmule_uti"] = 0
            return result
        pass

    def mla_ofdp(self, cfg, data, name, dry_run = False):
        #[TODO] Implementation
        # Check Configuration and Automatic Tiling/Scheduling
        app_path = self.kernel_root / "SummaGEMM"

        # Naive check and do automatic parameterization
        cfg, repeat = gemm_optimizer.ofdp_opt(cfg, self.arch)
        self.record_info({f"{name}" : cfg}, subdir="kernels")
        result = {}
        gemm_flop = 2 * cfg.m_size * cfg.n_size * cfg.k_size
        result['FLOP'] = gemm_flop

        # Generate Configuration
        gemm_cfg_h = self.kernel_root / "SummaGEMM" / "include" / "gemm.h"
        kc.generate_config_C_header("GEMM", cfg, gemm_cfg_h, cfg.dtype, cfg.summa_numer)

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

        if not dry_run:
            # Execute SoftHier Simulation
            cmd = f"make -C {self.softhier_root} {self.run_option} > {self.output_folder_trace}/{name}.log 2>&1"
            # print(f"[System Call] {cmd}")
            assert os.system(cmd) == 0

            # Anaylze Result
            runtime = self.get_runtime_ns(f"{self.output_folder_trace}/{name}.log") * repeat
            cycles = runtime * self.arch.cycles_per_ns
            peak_flop_per_cycle = 2 * self.arch.num_cluster_x * self.arch.num_cluster_y * self.arch.redmule_ce_height * self.arch.redmule_ce_width
            achieved_flop_per_cycle = gemm_flop / cycles
            redmule_uti = achieved_flop_per_cycle / peak_flop_per_cycle
            elem_size = 1 if cfg.dtype == 'fp8' else 2
            arithmetic_intensity = gemm_flop / (elem_size * (cfg.m_size * cfg.k_size + cfg.n_size * cfg.k_size + cfg.m_size * cfg.n_size * cfg.ofdp_splitk_num))
            result["runtime"] = runtime
            result["peak_flop_per_cycle"] = peak_flop_per_cycle
            result["achieved_flop_per_cycle"] = achieved_flop_per_cycle
            result["redmule_uti"] = redmule_uti
            result["arithmetic_intensity"] = arithmetic_intensity
            pass
        return result
        pass
        