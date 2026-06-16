#!/usr/bin/env python3

import gv.gvsoc_control as gvsoc
import queue
import re
import shlex
import threading
import time
import os
import shutil
import random
import subprocess
import signal
import ficlib.fic_proxy_helpers as helpers
import ficlib.poi_helpers as poi_helpers

from ficlib.fault_helpers import *
from dataclasses import dataclass

# We hash if the size is larger than this
# Leave it at that for now!
HASH_SIZE_THR       = 0

# In case fault corrupts code and gvsoc gets stuck
WATCHDOG_MULT       = 5

# For gvsoc instance launch
TIMEOUT             = 10

GVSOC_RUN_SUCCESS   = 0
GVSOC_RUN_ERROR     = -1
GVSOC_RUN_TIMEOUT   = -2

RUN_MASKED          = 0
RUN_PROPAGATED      = -1
RUN_NONTERM         = -2
RUN_STUCK           = -3

HASHES_DIFFER       = -1

GOLDEN_RUN_TID      = 777
GOLDEN_RUN_RID      = 777

@dataclass
class Cache:
    target:     int
    fic:        str
    path:       str
    nb_lines:   int
    line_size:  int
    tag_bits:   int
    entry_bits: int = 0
    total_bits: int = 0

    def __post_init__(self):
        self.entry_bits = 8 * self.line_size + self.tag_bits + 1
        self.total_bits = self.entry_bits * self.nb_lines

@dataclass
class CampaignManager:
    """
    All devices are ID by their paths, hence all `str`
    """
    pois:               list
    fics:               list

    target:             str 
    binary:             str
    builddir:           str

    fault_generator:    callable = None
    
    # FICs may run under different clocks
    golden_cycles:      dict[str, int] = None
    golden_runtime_s:   int | None = None

    all_devices:        list = None
    all_mems:           list = None
    all_regs:           list = None
    all_prefs:          list = None
    all_caches:         list = None

    regs:               list = None
    fregs:              list = None
    vregs:              list = None

    # All memories attached to FIC
    fic_to_mems:        dict[str, list] = None
    # Hash commands per every FIC
    fic_to_hashcmds:    dict[str, list] = None

    # All registers attached to FIC
    fic_to_xregs:       dict[str, list] = None

    # All prefetchers attached to FIC
    fic_to_prefs:       dict[str, list] = None

    # What is the size of this memory?
    mem_to_size:        dict[str, int] = None
    # With which FIC is this memory registerd?
    mem_to_fic:         dict[str, str] = None
    # What is the FIC relative ID of this memory?
    mem_to_target:      dict[str, int] = None

    # Byte width of regs of this regfile
    xreg_to_size:       dict[str, int] = None
    # How many regs does this regfile have?
    xreg_to_num:        dict[str, int] = None
    # Type of reg (reg, freg, vreg)
    xreg_to_kind:       dict[str, str] = None
    xreg_to_fic:        dict[str, str] = None
    xreg_to_target:     dict[str, int] = None

    pref_to_size:       dict[str, int] = None
    pref_to_fic:        dict[str, str] = None
    pref_to_target:     dict[str, int] = None
    
    fic_to_caches:      dict[str, list] = None

    # Sanitized FIC paths for file handling
    san_fics:           dict[str, str] = None

    targetdir:          str = ""
    config_opts:        str = ""

    threads:            int = 8
    total_runs:         int = 80

    worker_threads:     list = None
    runs_remaining:     list = None
    faulty_runs:        list = None

    print_injections:   bool = False
    print_details:      bool = True

    def __post_init__(self):
        self.fic_to_mems        = {}
        self.fic_to_hashcmds    = {}
        self.fic_to_xregs       = {}
        self.fic_to_prefs       = {}

        self.mem_to_size        = {}
        self.mem_to_fic         = {}
        self.mem_to_target      = {}

        self.xreg_to_size       = {}
        self.xreg_to_num        = {}
        self.xreg_to_kind       = {}
        self.xreg_to_fic        = {}
        self.xreg_to_target     = {}

        self.pref_to_size       = {} 
        self.pref_to_fic        = {} 
        self.pref_to_target     = {} 

        self.fic_to_caches      = {}

        self.san_fics           = {}
        self.golden_cycles      = {}

        for fic in self.fics:
            san_fic = fic.replace('/', '_')
            self.san_fics[fic] = san_fic

        for i, poi in enumerate(self.pois):
            if poi.target is None:
                raise ValueError('All PoI must indicate the target!')
            cfic = poi.checker_path
            if cfic is None:
                raise ValueError('All PoI must indicate which FIC checks them!')
            if cfic not in self.fic_to_hashcmds:
                self.fic_to_hashcmds[cfic] = []
            req = final_hash_req_str(poi.target, poi.addr, poi.size, i)
            self.fic_to_hashcmds[cfic].append(req)

        self.faulty_runs = [None] * self.threads
        self.runs_remaining = [0] * self.threads
        
        os.makedirs(f'{self.builddir}/faults', exist_ok=True)

        # Distribute work across threads in naive way
        for i in range(self.threads):
            self.runs_remaining[i] = int(self.total_runs/self.threads)
        if self.total_runs % self.threads != 0:
            self.runs_remaining[self.threads - 1] += self.total_runs % self.threads 

    def run_gvsoc(self, tid, run_id, fault_opts=None, golden_run=False):
        cmd = [
            f"{self.builddir}/install/bin/gvsoc",
            f"--target={self.target}",
            f"--work-dir={self.builddir}/work_{tid}",
            f"--binary={self.binary}"
        ]   

        if self.targetdir != "":
            cmd += [f"--target-dir={self.targetdir}"]

        # Add target configurations
        cmd += shlex.split(self.config_opts)

        # Add faults files if any
        if fault_opts is not None:
            cmd += fault_opts

        cmd += ["run"]

        try:
            # Run as a group to kill zombie children together with their parents
            proc = subprocess.Popen(cmd, preexec_fn=os.setsid)

            if not golden_run:
                proc.wait(timeout=self.golden_runtime_s * WATCHDOG_MULT)
                if proc.returncode == GVSOC_RUN_SUCCESS:
                    #print(f'{tid}-{run_id}: run success {proc.returncode}')
                    return GVSOC_RUN_SUCCESS
                else:
                    print(f'{tid}-{run_id}: error code {proc.returncode}')
                    return GVSOC_RUN_ERROR
            else:
                proc.wait()
                if proc.returncode != GVSOC_RUN_SUCCESS:
                    print(f'Golden run exited with error code {proc.returncode}')
                    exit(1)
        except subprocess.TimeoutExpired:
            print(f'{tid}-{run_id}: caught TimeoutExpired -> killing process group!')
            if proc:
                os.killpg(proc.pid, signal.SIGKILL)  # kill whole group to avoid zombies
            return GVSOC_RUN_TIMEOUT
        finally:
            try:
                os.killpg(proc.pid, signal.SIGKILL)  # kill whole group to avoid zombies
            except:
                pass

    def do_golden_run(self):
        cmd_line = []

        # It would be more consise to do this using 
        # proxy instead of files. But this is robust.
        for fic in self.fics:
            san_fic = self.san_fics[fic]
            ffp = f'{self.builddir}/faults/{san_fic}'
            with open(ffp, 'w') as ff:
                if fic in self.fic_to_hashcmds:
                    for req in self.fic_to_hashcmds[fic]:
                        ff.write(f'{req}\n')
                req = final_cycle_count_req_str()
                ff.write(f'{req}\n')
                req = final_mem_data_req_str()
                ff.write(f'{req}\n')
                req = final_reg_data_req_str()
                ff.write(f'{req}\n')
                req = final_prefs_data_req_str()
                ff.write(f'{req}\n')
                req = final_caches_data_req_str()
                ff.write(f'{req}\n')

            cmd_line += [f'--config-opt={fic}/faults_path={ffp}']
        
        start_time = time.perf_counter()
        self.run_gvsoc(GOLDEN_RUN_TID, GOLDEN_RUN_RID, fault_opts=cmd_line, golden_run=True)
        end_time = time.perf_counter()

        self.golden_runtime_s = end_time - start_time

        self.all_devices    = []
        self.all_mems       = []
        self.all_regs       = []
        self.all_prefs      = []
        self.all_caches     = []

        self.regs   = []
        self.fregs  = []
        self.vregs  = []

        for fic in self.fics:
            # Cycle count
            san_fic = self.san_fics[fic]
            ccp = f'{self.builddir}/work_{GOLDEN_RUN_TID}/cycle_count_{san_fic}'
            with open(ccp, 'r') as ccf:
                for line in ccf:
                    cc = int(line.strip())
                    self.golden_cycles[fic] = cc

            # Memories
            self.fic_to_mems[fic] = []
            mdp = f'{self.builddir}/work_{GOLDEN_RUN_TID}/memories_data_{san_fic}'
            with open(mdp, 'r') as mdf:
                for line in mdf:
                    target_s, dev, size_s = line.split()
                    target = int(target_s)
                    size = int(size_s)
                    self.fic_to_mems[fic].append(dev)
                    self.mem_to_size[dev] = size
                    self.mem_to_fic[dev] = fic
                    self.mem_to_target[dev] = target
                    self.all_devices.append(dev)
                    self.all_mems.append(dev)

            # Regfiles
            self.fic_to_xregs[fic] = []
            rdp = f'{self.builddir}/work_{GOLDEN_RUN_TID}/regfiles_data_{san_fic}'
            with open(rdp, 'r') as rdf:
                for line in rdf:
                    t_s, dev, k_s, s_s, n_s = line.split()
                    target = int(t_s)
                    kind = int(k_s)
                    size = int(s_s)
                    num = int(n_s)
                    if kind == 0:
                        self.regs.append(dev)
                        kind_s = "reg"
                    if kind == 1:
                        self.fregs.append(dev)
                        kind_s = "freg"
                    elif kind == 2:
                        self.vregs.append(dev)
                        kind_s = "vreg"
                    dev = dev + "_" + kind_s
                    self.fic_to_xregs[fic].append(dev)
                    self.xreg_to_size[dev] = size
                    self.xreg_to_num[dev] = num
                    self.xreg_to_kind[dev] = kind_s
                    self.xreg_to_fic[dev] = fic
                    self.xreg_to_target[dev] = target
                    self.all_devices.append(dev)
                    self.all_regs.append(dev)

            # Prefetchers
            self.fic_to_prefs[fic] = []
            pdp = f'{self.builddir}/work_{GOLDEN_RUN_TID}/prefetchers_data_{san_fic}'
            with open(pdp, 'r') as pdf:
                for line in pdf:
                    t_s, dev, s_s, = line.split()
                    target = int(t_s)
                    size = int(s_s)
                    dev = dev + "_pref"
                    self.fic_to_prefs[fic].append(dev)
                    self.pref_to_size[dev] = size
                    self.pref_to_fic[dev] = fic
                    self.pref_to_target[dev] = target
                    self.all_devices.append(dev)
                    self.all_prefs.append(dev)

            # Caches
            self.fic_to_caches[fic] = []
            cdp = f'{self.builddir}/work_{GOLDEN_RUN_TID}/caches_data_{san_fic}'
            with open(cdp, 'r') as cdf:
                for line in cdf:
                    t_s, dev, nbl_s, ls_s, tb_s = line.split()
                    target = int(t_s)
                    nb_lines = int(nbl_s)
                    line_size = int(ls_s)
                    tag_bits = int(tb_s)
                    cache = Cache(
                        target=target,
                        fic=fic,
                        path=dev,
                        nb_lines=nb_lines,
                        line_size=line_size,
                        tag_bits=tag_bits,
                    )
                    self.fic_to_caches[fic].append(cache)
                    self.all_caches.append(cache)
                    # Doesnt go into all_devices?

            # Hashes (if any)
            if fic in self.fic_to_hashcmds:
                hp = f'{self.builddir}/work_{GOLDEN_RUN_TID}/hashes_{san_fic}'
                with open(hp, 'r') as hf:
                    for line in hf:
                        id_s, h_s = line.split()
                        id = int(id_s)
                        h = int(h_s)
                        self.pois[id].value = h

            if os.path.exists(f'{self.builddir}/faults/{san_fic}'):
                os.remove(f'{self.builddir}/faults/{san_fic}')

        if os.path.exists(f'{self.builddir}/work_{GOLDEN_RUN_TID}'):
            shutil.rmtree(f'{self.builddir}/work_{GOLDEN_RUN_TID}')
    
    def worker(self, tid):
        self.faulty_runs[tid] = []
        current_run = 0

        if self.fault_generator is None:
            raise RuntimeError('fault_generator must be supplied before starting worker threads!')

        while self.runs_remaining[tid] > 0:
            faults = self.fault_generator(tid, current_run)

            # DEBUG map paths to faults
            for i, f in enumerate(faults):
                f.id = i
                if f.target_type == 0:
                    f.device = self.fic_to_mems[f.fic][f.target]
                elif f.target_type == 1:
                    f.device = self.fic_to_xregs[f.fic][f.target]
                elif f.target_type == 2:
                    f.device = self.fic_to_prefs[f.fic][f.target]
                elif 3 <= f.target_type <= 5:
                    f.cache = self.fic_to_caches[f.fic][f.target]
                    f.device = f.cache.path

            fic_to_faults = get_fic_to_faults(faults)

            all_fic = set(fic_to_faults) | set(self.fic_to_hashcmds)
            
            cmd_parts = []

            # Fill the fault files per FIC and build up the command line
            for fic in all_fic:
                lines = []
                if fic in fic_to_faults:
                    lines.extend(f.format_string() for f in fic_to_faults[fic])
                if fic in self.fic_to_hashcmds:
                    lines.extend(self.fic_to_hashcmds[fic])
                san_fic = self.san_fics[fic]
                ffp = f"{self.builddir}/faults/{san_fic}_{tid}"
                with open(ffp, 'w') as ff:
                    ff.write('\n'.join(lines))
                cmd_parts.append(f'--config-opt={fic}/faults_path={ffp}')

            status = self.run_gvsoc(tid, current_run, fault_opts=cmd_parts)

            str_id = f"{tid}-{current_run}"

            if status == GVSOC_RUN_ERROR or status == GVSOC_RUN_TIMEOUT:
                self.faulty_runs[tid].append([str_id, status, faults, []])
            else:
                # Load in the hashes
                corrupted_pois = []
                for fic in self.fic_to_hashcmds:
                    san_fic = self.san_fics[fic]
                    hp = f"{self.builddir}/work_{tid}/hashes_{san_fic}"
                    with open(hp, 'r') as hf:
                        for line in hf:
                            id_s, h_s = line.split()
                            id = int(id_s)
                            h = int(h_s)
                            if self.pois[id].value != 0 and self.pois[id].value != h:
                                corrupted_pois.append([id, h])

                if len(corrupted_pois) > 0:
                    self.faulty_runs[tid].append([str_id, GVSOC_RUN_SUCCESS, faults, corrupted_pois])

            self.runs_remaining[tid] = self.runs_remaining[tid] - 1
            current_run = current_run + 1

        # Tidy up 
        for fic in self.fics:
            san_fic = self.san_fics[fic]
            if os.path.exists(f'{self.builddir}/faults/{san_fic}_{tid}'):
                os.remove(f'{self.builddir}/faults/{san_fic}_{tid}')

        if os.path.exists(f'{self.builddir}/work_{tid}'):
            shutil.rmtree(f'{self.builddir}/work_{tid}')

    def start_workers(self):
        """
        Should be pool managed. It works this way pretty well though.
        """
        self.worker_threads = []
            
        for i in range(self.threads):
            t = threading.Thread(target=self.worker, args=(i,))
            t.start()
            self.worker_threads.append(t)

        for i in range(self.threads):
            self.worker_threads[i].join()
            print(f"joined thread {i} out if {self.threads}")

    def get_matching_devices(self, list, regex):
        return [ dev for dev in list if re.match(regex, dev) ]         

    def print_results(self):
        g_masked    = 0
        g_sdc       = 0
        g_due       = 0
        g_sim_crash = 0

        all_faulty_runs = [x for xs in self.faulty_runs if xs for x in xs]

        # First, print the valid PoI values
        for id, exit_code, faults, corrupted_pois in all_faulty_runs:
            if exit_code == GVSOC_RUN_TIMEOUT:
                g_sim_crash = g_sim_crash + 1 
                if self.print_details:
                    print(f'\t{id}: PROCESS HIT TIMEOUT (SIMULATION HANG/CRASH)')
            elif exit_code == GVSOC_RUN_ERROR:
                g_due = g_due + 1
                if self.print_details:
                    print(f'\t{id}: SIMULATION EXITED WITH ERROR (DUE)')
            else:
                g_sdc = g_sdc + 1
                if self.print_details:
                    print(f'\t{id}: FAULTS PROPAGATED INTO POIs (SDC)')
                    for ind, val in corrupted_pois:
                            print(f'\t\tPOI_ID {ind}: {self.pois[ind]} hash differs (got {val} but expected {self.pois[ind].value})')
                    print(f'\t\t===> {len(corrupted_pois)}/{len(self.pois)} PoIs are faulty')

            if self.print_injections:
                fi_id_width = len(str(len(faults) - 1))
                for ind, fault in enumerate(faults):
                    print(f'\t\t\tFI_ID {fault.id:0{fi_id_width}d}: {injection_str(fault)}')


        g_masked = self.total_runs - g_sdc - g_sim_crash - g_due

        print( f'\nOUT OF {self.total_runs} RUNS: {g_masked} MASKED, {g_sdc} SDC, {g_due} DUE, {g_sim_crash} SIMULATOR FAULTED')
