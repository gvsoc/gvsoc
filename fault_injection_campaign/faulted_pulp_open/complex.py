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

# Probability of bit in register flipping each cycle
P_BITFLIP_REG           = 5e-11

# Idem for prefetcher buffer
P_BITFLIP_PREF          = 1e-10

# Idem for L0 caches
P_BITFLIP_L0            = 1e-10

# Duration of the stuck_at fault
# (Model permanent fault with \infty?)
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
BINARY                  = os.path.join(os.getcwd(), "faulted_pulp_open/build/test/test")

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

print(f'The following {len(campaign.pois)} locations will be checked for integrity:')
for poi in campaign.pois:
    if poi.value != 0:
        print(f'\t{poi}')
    else:
        print(f'\t{poi} UNREADABLE => ignored!')

""" Define the fault generator function """
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

        # smth with Poisson distribution idk
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


    """ Regfiles """
    bits_x_cycles = 0
    for regfile in campaign.all_regs:
        size    = campaign.xreg_to_size[regfile]
        num     = campaign.xreg_to_num[regfile]
        fic     = campaign.xreg_to_fic[regfile]
        cycles  = campaign.golden_cycles[fic]
        bits_x_cycles += 8 * size * num * cycles


    lam = P_BITFLIP_REG * bits_x_cycles
    nb_reg_faults = np.random.poisson(lam)

    for _ in range(nb_reg_faults):
        # Choose random parameters for the bitflip
        id = random.randint(0, len(campaign.all_regs) - 1)
        regfile = campaign.all_regs[id]
        fic = campaign.xreg_to_fic[regfile]
        target = campaign.xreg_to_target[regfile]
        # Yes, include the $0 register as well
        reg = random.randint(0, campaign.xreg_to_num[regfile] - 1)
        cycle = random.randint(0, campaign.golden_cycles[fic] - 1)

        coin = random.random()
        if coin < P_2_BIT_UPSET:
            bit = random.randint(0, campaign.xreg_to_size[regfile] - 2)
            fault = reg_mbu_req(target, reg, bit, cycle, 2, fic=fic, path=regfile)
        elif 1 - coin < P_3_BIT_UPSET:
            bit = random.randint(0, campaign.xreg_to_size[regfile] - 3)
            fault = reg_mbu_req(target, reg, bit, cycle, 3, fic=fic, path=regfile)
        else:
            bit = random.randint(0, campaign.xreg_to_size[regfile] - 1)
            fault = reg_bitflip_req(target, reg, bit, cycle, fic=fic, path=regfile)
        faults.append(fault)

    """ Prefetcher buffers """
    bits_x_cycles = 0
    for pref in campaign.all_prefs:
        size    = campaign.pref_to_size[pref]
        fic     = campaign.pref_to_fic[pref]
        cycles  = campaign.golden_cycles[fic]
        bits_x_cycles += 8 * size * cycles

    lam = P_BITFLIP_PREF * bits_x_cycles
    nb_pref_faults = np.random.poisson(lam)

    for _ in range(nb_pref_faults):
        # Choose random parameters for the bitflip
        pref_id = random.randint(0, len(campaign.all_prefs) - 1)
        pref = campaign.all_prefs[pref_id]
        fic = campaign.pref_to_fic[pref]
        target = campaign.pref_to_target[pref]
        byte = random.randint(0, campaign.pref_to_size[pref] - 1)
        cycle = random.randint(0, campaign.golden_cycles[fic] - 1)

        coin = random.random()
        if coin < P_2_BIT_UPSET:
            bit = random.randint(0, 8 - 2)
            fault = pref_mbu_req(target, byte, bit, cycle, 2, fic=fic, path=pref)
        elif 1 - coin < P_3_BIT_UPSET:
            bit = random.randint(0, 8 - 3)
            fault = pref_mbu_req(target, byte, bit, cycle, 3, fic=fic, path=pref)
        else:
            bit = random.randint(0, 8 - 1)
            fault = pref_bitflip_req(target, byte, bit, cycle, fic=fic, path=pref)
        faults.append(fault)


    total_bits = 0
    bits_x_cycles = 0
    for cache in campaign.all_caches:
        total_bits += cache.total_bits
        bits_x_cycles += cache.total_bits * campaign.golden_cycles[cache.fic]

    lam = P_BITFLIP_L0  * bits_x_cycles
    nb_l0_faults = np.random.poisson(lam)
    for _ in range(nb_l0_faults):
        bit = random.randint(0, total_bits - 1) 
        cache_id = 0
        while campaign.all_caches[cache_id].total_bits <= bit:
            bit -= campaign.all_caches[cache_id].total_bits
            cache_id += 1
        
        # Now we know into which cache we fall and offset 
        # within it. Need to figure out where exactly
        cache = campaign.all_caches[cache_id]
        fic = cache.fic
        cycle = random.randint(0, campaign.golden_cycles[fic] - 1)
        entry_nr = bit // cache.entry_bits
        entry_off = bit % cache.entry_bits

        if entry_off < cache.tag_bits: 
            # Tag
            fault = cache_tag_bitflip_req(
                cache.target, entry_nr, entry_off, cycle, fic=fic, path=cache.path
            )
        elif entry_off < cache.tag_bits + 1:
            # Dirty bit
            fault = cache_dirty_bitflip_req(
                cache.target, entry_nr, cycle, fic=fic, path=cache.path
            )
        else:
            # Data
            byte = (entry_off - cache.tag_bits - 1) // 8
            bit = (entry_off - cache.tag_bits - 1) % 8
            fault = cache_data_bitflip_req(
                cache.target, cache.line_size, entry_nr, byte, bit, 
                cycle, fic=fic, path=cache.path
            )

        faults.append(fault)

    return faults

""" Must set the fault generator before starting campaign """
campaign.fault_generator = fault_generator

campaign.start_workers()

campaign.print_results()

""" By now we have a lot of interesting data (further analysis?) """

exit(0)
