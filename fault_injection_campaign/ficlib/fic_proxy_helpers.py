#!/usr/bin/env python3

from dataclasses import dataclass

@dataclass
class FIC_meta:
    path: str
    id: int

def make_FIC_meta(gv, path):
    return FIC_meta(path, gv._get_component(path))

def print_cycles(gv, fic_m):
    print(f'{fic_m.path}: cycle={get_cycles(gv, fic_m.path)}')

def print_period(gv, fic_m):
    print(f'{fic_m.path}: period={get_period(gv, fic_m.path)}')

def print_number_memories(gv, fic_m):
    print(f'[{fic_m.path}] nb_memories={get_number_memories(gv, fic_m.path)}')

def print_memory_size(gv, fic_m, mem_id):
    print(f'[{fic_m.path}] mem_{mem_id}_size={get_memory_size(gv, fic_m.path, mem_id)}')

def send_read_cmd(gv, cmd):
    req = gv._send_cmd(cmd, keep_lock=True, wait_reply=False)
    reply = gv.reader._get_payload(req)
    gv._unlock_cmd()
    gv.reader.wait_reply(req)
    return reply

def get_nb_memories(gv, fic_path):
    component = gv._get_component(fic_path)
    cmd = f'component {component} get_nb_memories'
    reply = send_read_cmd(gv, cmd)
    return int.from_bytes(reply, byteorder='little')

def get_memory_size(gv, fic_path, mem_id):
    component = gv._get_component(fic_path)
    cmd = f'component {component} get_memory_size {mem_id}'
    reply = send_read_cmd(gv, cmd)
    return int.from_bytes(reply, byteorder='little')

def get_cycles(gv, fic_path):
    component = gv._get_component(fic_path)
    cmd = f'component {component} get_cycles'
    reply = send_read_cmd(gv, cmd)
    return int.from_bytes(reply, byteorder='little')

def get_period(gv, fic_path):
    component = gv._get_component(fic_path)
    cmd = f'component {component} get_period'
    reply = send_read_cmd(gv, cmd)
    return int.from_bytes(reply, byteorder='little')

def query_hashes(gv, cmd):
    reply = send_read_cmd(gv, cmd)
    chunks = [reply[i:i+8] for i in range(0, len(reply), 8)]
    return chunks

def get_hashes(gv, fic_path, pois):
    component = gv._get_component(fic_path)
    cmd = f'component {component} hash'
    for poi in pois:
        cmd += f' {poi.target} {poi.addr} {poi.size}'
    return query_hashes(gv, cmd)
