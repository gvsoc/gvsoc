#!/usr/bin/env python3

import argparse
import gv.gvsoc_control as gvsoc
import threading
import time
import os
import sys
import random
import pexpect
import ficlib.poi_helpers as poi_helpers
import numpy as np

from ficlib.fault_helpers import *
from ficlib.campaign_manager import *

""" Numbers taken from my imagination """

# We get 1 fault per 1 KB every X cycles
L1_FAULT_RATE           = 10_000_000
L2_PRIV_FAULT_RATE      = 5_000_000
L2_SHARED_FAULT_RATE    = 3_000_000

# A fault is stuck-at w.p. 0.15 else SEU (bitflip)
P_STUCKAT               = 0.15

# Given singe-event-upset, probability of it being 
# 2/3-bit upset, else it is usual 1-bitflip
P_2_BIT_UPSET           = 0.08
P_3_BIT_UPSET           = 0.02

# Duration of the stuck_at fault
T_DURATION_MIN          = 10
T_DURATION_MAX          = 10000

NB_PE                   = 8
NB_CLUSTER              = 4

PRINT_INJECTIONS        = True
PRINT_DETAILS           = True

THREADS                 = 12
TOTAL_RUNS              = 120

""" WORKAROUND """
BUILD_DIR               = os.path.join(os.getcwd(), "faulted_pulp_open/build")
WORK_DIR                = os.path.join(os.getcwd(), "faulted_pulp_open/build/work")
BINARY                  = os.path.join(os.getcwd(), "faulted_pulp_open/build/test/binary")

fic = 'chip/soc/fic'

""" Specify the points of interest """
# Do integrity check on all ELF symbols. This call will do the parsing for us.
pois = poi_helpers.find_pois(BINARY, select=poi_helpers.ALL_SYMBOLS, ignore=[])

# Do integrity checks with `chip/soc/fic_soc` using global address space
# (For now we can do integrity checks via global address space only!)
for poi in pois:
    poi.checker_path = fic
    poi.target       = -1

""" That is the core """
campaign = CampaignManager(
    pois=pois,
    fics=[fic],
    target="pulp-open",
    binary=BINARY,
    builddir=BUILD_DIR,
    config_opts=f'--config-opt=cluster/nb_pe={NB_PE} --config-opt=**/nb_cluster={NB_CLUSTER}',
    print_injections=PRINT_INJECTIONS,
    print_details=PRINT_DETAILS,
    threads=THREADS,
    total_runs=TOTAL_RUNS
)

""" This will supply reference values and relevant data (e.g. memory sizes) """
campaign.do_golden_run()

""" We want to target the following devices """
l1_banks        = campaign.get_matching_devices(campaign.all_mems, r".*l1.*")
l2_priv         = campaign.get_matching_devices(campaign.all_mems, r".*priv.*")
l2_shared_cuts  = campaign.get_matching_devices(campaign.all_mems, r".*shared.*")

"""
print(f'The following {len(campaign.pois)} locations will be checked for integrity:')
for poi in campaign.pois:
    if poi.value != 0:
        print(f'\t{poi}')
    else:
        print(f'\t{poi} UNREADABLE => ignored!')
"""

""" Define the fault generating function """
def fault_generator(tid, run_id):
    faults = []

    """ Memories """
    for dev in l1_banks + l2_priv + l2_shared_cuts:
        # Metadata of the current device
        size    = campaign.mem_to_size[dev]
        target  = campaign.mem_to_target[dev]
        fic     = campaign.mem_to_fic[dev]
        cycles  = campaign.golden_cycles[fic]

        if dev in l1_banks:
            fault_rate = L1_FAULT_RATE
        elif dev in l2_priv:
            fault_rate = L2_PRIV_FAULT_RATE
        else:
            fault_rate = L2_SHARED_FAULT_RATE

        lam = (size / (1024)) * (cycles / fault_rate)
        nb_mem_faults = np.random.poisson(lam)

        for _ in range(nb_mem_faults):
            cycle   = random.randint(0, cycles)
            pos     = random.randint(0, size - 1)

            if random.random() < P_STUCKAT:
                duration = random.randint(T_DURATION_MIN, T_DURATION_MAX)
                is_high = random.randint(0,1)
                bit = random.randint(0, 7)
                fault = mem_intermittent_req(target, pos, bit, cycle, fic=fic,
                    duration=duration, is_high=is_high, path=dev)
            else:
                coin = random.random()
                if coin < P_2_BIT_UPSET:
                    bit = random.randint(0, 7 - 1)
                    fault = mem_mbu_req(target, pos, bit, cycle, 2, fic=fic, path=dev)
                elif 1 - coin < P_3_BIT_UPSET:
                    bit = random.randint(0, 7 - 2)
                    fault = mem_mbu_req(target, pos, bit, cycle, 3, fic=fic, path=dev)
                else:
                    bit = random.randint(0, 7)
                    fault = mem_bitflip_req(target, pos, bit, cycle, fic=fic, path=dev)

            faults.append(fault)

    return faults

""" Must set the fault generator before starting campaign """
campaign.fault_generator = fault_generator

campaign.start_workers()

campaign.print_results()

exit(0)
