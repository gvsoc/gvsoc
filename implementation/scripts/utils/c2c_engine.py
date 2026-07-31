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
import utils.common as common

class C2CEngine(object):
    """docstring for C2CEngine"""
    def __init__(self, softhier_root, output_root, tag=None):
        super(C2CEngine, self).__init__()
        self.softhier_root = Path(softhier_root).resolve()
        self.output_root = Path(output_root).resolve()
        if not self.softhier_root.exists():
            raise RuntimeError(f"SoftHier Root Not Exist at : {softhier_root}")
            pass
        if not self.output_root.exists():
            raise RuntimeError(f"Output Root Not Exist at : {output_root}")
            pass
        prefix = "c2c_" if tag == None else f"{tag}_"
        output_name = prefix + datetime.now().strftime("%Y%m%d_%H%M%S")
        self.output_folder = self.output_root / output_name
        os.system(f"mkdir -p {self.output_folder}")
        print(f"[bold green]Output Folder Created at : {self.output_folder}[/bold green]")
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

    def register_workload(self, name, info_dict):
        folder_name = re.sub(r'[^\w.\-]', '_', name)
        self.output_folder_cfg = self.output_folder / folder_name / "cfg"
        self.output_folder_trace = self.output_folder / folder_name / "trace"
        self.output_folder_info = self.output_folder / folder_name / "info"
        os.system(f"mkdir -p {self.output_folder_cfg}")
        os.system(f"mkdir -p {self.output_folder_trace}")
        os.system(f"mkdir -p {self.output_folder_info}")
        self.record_info(info_dict)
        assert 'ccfg' in info_dict, "C2C Platform Configuration Missing in Info Dict"
        self.c2c_cfg = info_dict['ccfg']
        common.write_class_file(self.c2c_cfg, self.output_folder_cfg / "c2c_platform_cfg.py")
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

    def execute_simulation(self, name):
        c2c_config_path = self.output_folder_cfg / "c2c_platform_cfg.py"
        cmd = f"ccfg={c2c_config_path} make -C {self.softhier_root} c2c-hw c2c-run > {self.output_folder_trace}/{name}.log 2>&1"
        assert os.system(cmd) == 0, f"C2C Simulation Failed, see log at {self.output_folder_trace}/{name}.log"
        return self.get_runtime_ns(self.output_folder_trace / f"{name}.log")
        pass

    def execute_all_gather(self, name, params, ccfg):
        assert params['parallelism'] == ccfg.num_chip, "All_Gather Parallelism Mismatch with Number of Chips"
        output_folder_trace_op = self.output_folder_trace / name
        os.system(f"mkdir -p {output_folder_trace_op}")
        # generate trace files by iterating over all chips
        for chip_id in range(ccfg.num_chip):
            base_name = output_folder_trace_op / f"all_gather_chip"
            file_name = f"{base_name}_{chip_id}.trace"
            with open(file_name, "w") as f:
                # iterate over all chips
                for i in range(ccfg.num_chip):
                    send_to_chip_id = (chip_id + i) % ccfg.num_chip
                    if send_to_chip_id == chip_id: continue
                    send_data_size = params['m_size'] * params['n_size'] * params['elem_size']
                    while send_data_size > 0:
                        flit_size = min(send_data_size, ccfg.flit_granularity_byte)
                        f.write(f"{send_to_chip_id}, {flit_size}\n")
                        send_data_size -= flit_size
                    pass
                # close file
                f.close()
            pass
        # update cfg to use trace files
        ccfg.use_trace_file = 1
        ccfg.trace_file_base = str(output_folder_trace_op / "all_gather_chip")
        common.write_class_file(ccfg, self.output_folder_cfg / "c2c_platform_cfg.py")

        # execute simulation
        return self.execute_simulation(name)
        pass

    def execute_all_to_all(self, name, params, ccfg):
        assert params['parallelism'] == ccfg.num_chip, "All_to_All Parallelism Mismatch with Number of Chips"
        output_folder_trace_op = self.output_folder_trace / name
        os.system(f"mkdir -p {output_folder_trace_op}")
        # generate trace files by iterating over all chips
        for chip_id in range(ccfg.num_chip):
            base_name = output_folder_trace_op / f"all_to_all_chip"
            file_name = f"{base_name}_{chip_id}.trace"
            with open(file_name, "w") as f:
                # iterate over all chips
                for i in range(ccfg.num_chip):
                    send_to_chip_id = (chip_id + i) % ccfg.num_chip
                    if send_to_chip_id == chip_id: continue
                    send_data_size = params['m_size'] * params['n_size'] * params['elem_size']
                    send_data_size = (send_data_size + ccfg.num_chip - 1) // ccfg.num_chip  # ceiling division
                    while send_data_size > 0:
                        flit_size = min(send_data_size, ccfg.flit_granularity_byte)
                        f.write(f"{send_to_chip_id}, {flit_size}\n")
                        send_data_size -= flit_size
                    pass
                # close file
                f.close()
            pass
        # update cfg to use trace files
        ccfg.use_trace_file = 1
        ccfg.trace_file_base = str(output_folder_trace_op / "all_to_all_chip")
        common.write_class_file(ccfg, self.output_folder_cfg / "c2c_platform_cfg.py")

        # execute simulation
        return self.execute_simulation(name)
        pass

    def execute_reduce_scatter(self, name, params, ccfg):
        assert params['parallelism'] == ccfg.num_chip, "Reduce_Scatter Parallelism Mismatch with Number of Chips"
        output_folder_trace_op = self.output_folder_trace / name
        os.system(f"mkdir -p {output_folder_trace_op}")
        # generate trace files by iterating over all chips
        for chip_id in range(ccfg.num_chip):
            base_name = output_folder_trace_op / f"reduce_scatter_chip"
            file_name = f"{base_name}_{chip_id}.trace"
            with open(file_name, "w") as f:
                # iterate over all chips
                for i in range(ccfg.num_chip):
                    send_to_chip_id = (chip_id + i) % ccfg.num_chip
                    if send_to_chip_id == chip_id: continue
                    send_data_size = params['m_size'] * params['n_size'] * params['elem_size']
                    send_data_size = (send_data_size + ccfg.num_chip - 1) // ccfg.num_chip  # ceiling division
                    while send_data_size > 0:
                        flit_size = min(send_data_size, ccfg.flit_granularity_byte)
                        f.write(f"{send_to_chip_id}, {flit_size}\n")
                        send_data_size -= flit_size
                    pass
                # close file
                f.close()
            pass
        # update cfg to use trace files
        ccfg.use_trace_file = 1
        ccfg.trace_file_base = str(output_folder_trace_op / "reduce_scatter_chip")
        common.write_class_file(ccfg, self.output_folder_cfg / "c2c_platform_cfg.py")

        # execute simulation
        return self.execute_simulation(name)
        pass

    def execute_dispatch(self, name, params, ccfg):
        assert params['parallelism'] == ccfg.num_chip, "Dispatch Parallelism Mismatch with Number of Chips"
        output_folder_trace_op = self.output_folder_trace / name
        os.system(f"mkdir -p {output_folder_trace_op}")
        # generate trace files by iterating over all chips
        for chip_id in range(ccfg.num_chip):
            base_name = output_folder_trace_op / f"dispatch_chip"
            file_name = f"{base_name}_{chip_id}.trace"
            D = params['index'][chip_id::ccfg.num_chip].flatten()
            with open(file_name, "w") as f:
                # iterate over indecis
                for idx in D:
                    send_to_chip_id = idx.item() % ccfg.num_chip
                    send_data_size = params['embeded_length'] * params['elem_size']
                    while send_data_size > 0:
                        flit_size = min(send_data_size, ccfg.flit_granularity_byte)
                        f.write(f"{send_to_chip_id}, {flit_size}\n")
                        send_data_size -= flit_size
                    pass
                # close file
                f.close()
            pass
        # update cfg to use trace files
        ccfg.use_trace_file = 1
        ccfg.trace_file_base = str(output_folder_trace_op / "dispatch_chip")
        common.write_class_file(ccfg, self.output_folder_cfg / "c2c_platform_cfg.py")

        # execute simulation
        return self.execute_simulation(name)
        pass

    def execute_combine(self, name, params, ccfg):
        assert params['parallelism'] == ccfg.num_chip, "Combine Parallelism Mismatch with Number of Chips"
        output_folder_trace_op = self.output_folder_trace / name
        os.system(f"mkdir -p {output_folder_trace_op}")
        # generate trace files by iterating over all chips
        for chip_id in range(ccfg.num_chip):
            base_name = output_folder_trace_op / f"combine_chip"
            file_name = f"{base_name}_{chip_id}.trace"
            D = params['index'][chip_id::ccfg.num_chip].flatten()
            with open(file_name, "w") as f:
                # iterate over indecis
                for idx in D:
                    send_to_chip_id = idx.item() % ccfg.num_chip
                    send_data_size = params['embeded_length'] * params['elem_size']
                    while send_data_size > 0:
                        flit_size = min(send_data_size, ccfg.flit_granularity_byte)
                        f.write(f"{send_to_chip_id}, {flit_size}\n")
                        send_data_size -= flit_size
                    pass
                # close file
                f.close()
            pass
        # update cfg to use trace files
        ccfg.use_trace_file = 1
        ccfg.trace_file_base = str(output_folder_trace_op / "combine_chip")
        common.write_class_file(ccfg, self.output_folder_cfg / "c2c_platform_cfg.py")

        # execute simulation
        return self.execute_simulation(name)
        pass

    def execute_flow(self, c2c_flow):
        results = {}
        total_runtime = 0
        for k, v in c2c_flow.items():
            if v['type'] == 'All_Gather':
                print(f"[bold blue]Executing All_Gather on {k}[/bold blue]")
                res = self.execute_all_gather(k, v, self.c2c_cfg)
                results[k] = res
                total_runtime += res
                pass
            elif v['type'] == 'All_to_All':
                print(f"[bold blue]Executing All_to_All on {k}[/bold blue]")
                res = self.execute_all_to_all(k, v, self.c2c_cfg)
                results[k] = res
                total_runtime += res
                pass
            elif v['type'] == 'Reduce_Scatter':
                print(f"[bold blue]Executing Reduce_Scatter on {k}[/bold blue]")
                res = self.execute_reduce_scatter(k, v, self.c2c_cfg)
                results[k] = res
                total_runtime += res
                pass
            elif v['type'] == 'Disp':
                print(f"[bold blue]Executing Dispatch on {k}[/bold blue]")
                res = self.execute_dispatch(k, v, self.c2c_cfg)
                results[k] = res
                total_runtime += res
                pass
            elif v['type'] == 'Comb':
                print(f"[bold blue]Executing Combine on {k}[/bold blue]")
                res = self.execute_combine(k, v, self.c2c_cfg)
                results[k] = res
                total_runtime += res
                pass
            else:
                raise RuntimeError(f"Unknown C2C Operation Type: {v['type']}")
                pass
        results['total'] = total_runtime
        self.record_info({"Results" : results})
        pass
