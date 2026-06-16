#!/usr/bin/env python3

from dataclasses import dataclass

@dataclass
class Fault:
    cmd:            int
    kind:           int = 0
    addr:           int = 0
    bit:            int = 0
    fic:            str | None = None   # path of FIC to be contacted
    delay:          int = 0
    is_high:        int = 0
    duration:       int = 0
    size:           int = 0             
    id:             int = 0             # denotes target type if cmd=0
    target_type:    int = 0             # mem=0,reg=1,pref=2
    target:         int = -1            
    path:           str | None = None   # path of the fault'd device

    def format_string(self):
        return f'{self.cmd} {self.kind} {self.target_type} {self.target} {self.addr} {self.bit} {self.duration} {self.is_high} {self.delay} {self.size} {self.id}'

def get_fic_to_faults(faults):
    """ Returns a map {FIC} => {faults it injects} """
    fic_to_faults = {}
    for f in faults:
        if f.fic not in fic_to_faults:
            fic_to_faults[f.fic] = []
        fic_to_faults[f.fic].append(f)
    return fic_to_faults

def injection_str(fault):
    if fault.kind == 0:
        if fault.target_type == 0:   # Mem
            return f'{fault.fic} -> {fault.device} [0x{fault.addr:08x}] {fault.size}-BIT UPSET w/ MASK={((1 << fault.size) - 1 << fault.bit):08b} @{fault.delay}' 
        elif fault.target_type == 1: # Reg
            return f'{fault.fic} -> {fault.device} [reg={fault.addr:02d}] {fault.size}-BIT UPSET at bits {fault.addr}-{fault.addr + fault.size - 1} @{fault.delay}'
        elif fault.target_type == 2: # Pref
            return f'{fault.fic} -> {fault.device} [byte={fault.addr:02d}] {fault.size}-BIT UPSET w/ MASK={((1 << fault.size) - 1 << fault.bit):08b} @{fault.delay}'
        elif fault.target_type == 3: # $data
            return f'{fault.fic} -> {fault.device} [entry={fault.addr // fault.cache.line_size} byte={hex(fault.addr % fault.cache.line_size)}] {fault.size}-BIT UPSET w/ MASK={((1 << fault.size) - 1 << fault.bit):08b} @{fault.delay}'
        elif fault.target_type == 4: # $tag
            return f'{fault.fic} -> {fault.device} TAG [entry={fault.addr}] {fault.size}-BIT UPSET at bits {fault.addr}-{fault.addr + fault.size - 1} @{fault.delay}'
        elif fault.target_type == 5: # $dirty_bit
            return f'{fault.fic} -> {fault.device} DIRTY BIT [entry={fault.addr}] @{fault.delay}'
    elif fault.kind == 1:
        return f'{fault.fic} -> {fault.device} [0x{fault.addr:08x}: bit={fault.bit}] STUCK AT {fault.is_high} @[{fault.delay}-{fault.delay + fault.duration}: {fault.duration}]'

def final_cycle_count_req():
    return Fault(cmd=3)

def final_cycle_count_req_str():
    fault = final_cycle_count_req()
    return fault.format_string()

def final_hash_req(target, addr, size, id):
    return Fault(cmd=1, target=target, addr=addr, size=size, id=id)

def final_hash_req_str(target, addr, size, id):
    fault = final_hash_req(target, addr, size, id)
    return fault.format_string()

def final_mem_data_req():
    return Fault(cmd=2)

def final_mem_data_req_str():
    fault = final_mem_data_req()
    return fault.format_string()

def final_reg_data_req():
    return Fault(cmd=4)

def final_reg_data_req_str():
    fault = final_reg_data_req()
    return fault.format_string()

def final_prefs_data_req():
    return Fault(cmd=6)

def final_prefs_data_req_str():
    fault = final_prefs_data_req()
    return fault.format_string()

def final_caches_data_req():
    return Fault(cmd=7)

def final_caches_data_req_str():
    fault = final_caches_data_req()
    return fault.format_string()

def mem_intermittent_req(target, addr, bit, delay, duration, is_high, fic=None, path=None):
    return Fault(cmd=0, kind=1, target=target, addr=addr, bit=bit, delay=delay,fic=fic, 
                 duration=duration, is_high=is_high, target_type=0, path=path)

def mem_permanent_req(target, addr, bit, duration, is_high, fic=None, path=None):
    return Fault(cmd=0, kind=1, target=target, addr=addr, bit=bit, delay=0,fic=fic, 
                 duration=duration, is_high=is_high, target_type=0, path=path)

def mem_mbu_req(target, addr, bit, delay, nb_bits, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=addr, bit=bit, delay=delay,
                 fic=fic, size=nb_bits, target_type=0, path=path)

def reg_mbu_req(target, reg, bit, delay, nb_bits, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=reg, bit=bit, delay=delay,
                 fic=fic, size=nb_bits, target_type=1, path=path)

def pref_mbu_req(target, byte, bit, delay, nb_bits, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=byte, bit=bit, delay=delay,
                 fic=fic, size=nb_bits, target_type=2, path=path)

def cache_data_mbu_req(target, line_size, line_nr, byte, bit, delay, nb_bits, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=(line_size * line_nr + byte), bit=bit,
                 delay=delay, fic=fic, size=nb_bits, target_type=3, path=path)

def cache_tag_mbu_req(target, line_nr, bit, delay, nb_bits, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=line_nr, bit=bit, delay=delay, fic=fic, 
                 size=nb_bits, target_type=4, path=path)

def mem_bitflip_req(target, addr, bit, delay, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=addr, bit=bit, delay=delay, 
                 fic=fic, size=1, target_type=0, path=path)

def reg_bitflip_req(target, reg, bit, delay, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=reg, bit=bit, delay=delay, 
                 fic=fic, size=1, target_type=1, path=path)

def pref_bitflip_req(target, byte, bit, delay, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=byte, bit=bit, delay=delay, 
                 fic=fic, size=1, target_type=2, path=path)

def cache_data_bitflip_req(target, line_size, line_nr, byte, bit, delay, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=(line_size * line_nr + byte), bit=bit,
                 delay=delay, fic=fic, size=1, target_type=3, path=path)

def cache_tag_bitflip_req(target, line_nr, bit, delay, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=line_nr, bit=bit, delay=delay, fic=fic, 
                 size=1, target_type=4, path=path)

def cache_dirty_bitflip_req(target, line_nr, delay, fic=None, path=None):
    return Fault(cmd=0, kind=0, target=target, addr=line_nr, bit=0, delay=delay, fic=fic, 
                 size=1, target_type=5, path=path)
