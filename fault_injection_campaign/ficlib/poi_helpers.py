#!/usr/bin/env python3

import os

from dataclasses import dataclass
from elftools.elf.elffile import ELFFile

ALL_SYMBOLS         = 0
ALL_FUNCTIONS       = 1
GLOBAL_FUNCTIONS    = 2
LOCAL_FUNCTIONS     = 3
ALL_VARIABLES       = 4
GLOBAL_VARIABLES    = 5
LOCAL_VARIABLES     = 6
ALL_GLOBALS         = 7
ALL_LOCALS          = 8

@dataclass
class PoI:
    name:               str
    addr:               int
    size:               int
    value:              int | None = None       
    checker_path:       str | None = None       # FIC that checks its integrity
    target:             int | None = None       # device ID from FIC's perspective
    is_symbol:          bool = False
    kind:               str | None = None
    binding:            str | None = None
    dev:                str | None = None       # Try to get the path of the device...

    def __str__(self):
        s = f'[0x{(self.addr):08x}-0x{(self.addr + self.size):08x}: {self.size:8}B] '
        if self.is_symbol:
            return s + f'{self.binding} {self.kind} "{self.name}"'
        elif self.name == '':
            return s + 'raw memory'
        else:
            return s + f'"{self.name}"'

kind_map = {
    'STT_OBJECT':   'variable',
    'STT_FUNC':     'function',
    'STT_COMMON':   'common_var',
    'STT_TLS':      'tls_var'
}

bind_map = {
    'STB_LOCAL':    'local',
    'STB_GLOBAL':   'global',
    'STB_WEAK':     'weak'
}

# some local helpers
def _kind_match(spec, kind_str):
    if spec == ALL_SYMBOLS:
        return True

    if kind_str == 'function' and (spec == ALL_FUNCTIONS or \
        spec == GLOBAL_FUNCTIONS or spec == LOCAL_FUNCTIONS):
        return True

    if (kind_str == 'variable' or kind_str == 'common_var'or \
         kind_str == 'tls_var') and (spec == ALL_VARIABLES or \
         spec == GLOBAL_VARIABLES or spec == LOCAL_VARIABLES):
        return True 
    
    return False

def _bind_match(spec, bind_str):
    if spec == ALL_SYMBOLS:
        return True

    if bind_str == 'local' and (spec == LOCAL_FUNCTIONS or \
        spec == LOCAL_VARIABLES or spec == ALL_LOCALS):
        return True

    if bind_str == 'global' and (spec == GLOBAL_FUNCTIONS or \
        spec == GLOBAL_VARIABLES or spec == ALL_GLOBALS):
        return True 
    
    return False

def make_poi(addr, size, name=""):
    return PoI(name=name, addr=addr, size=size) 

def make_pois(list):
    poi_list = []
    for entry in list:
        if len(entry) == 2:
            addr, size = entry
            poi_list.append(make_poi(addr, size))
        elif len(entry) == 3:
            addr, size, name = entry
            poi_list.append(make_poi(addr, size, name=name))
    return poi_list

def find_pois(binary_path, select=ALL_SYMBOLS, names=None, ignore=None):
    if not os.path.exists(binary_path):
        raise RuntimeError(f'The file {binary_path} does not exist!')

    with open(binary_path, 'rb') as f:
        elf = ELFFile(f)

        symtab = elf.get_section_by_name('.symtab')

        if not symtab:
            raise RuntimeError("The binary is stripped and has no symbol table!")

        pois = []

        for sym in symtab.iter_symbols():
            name = sym.name
            addr = sym['st_value']
            size = sym['st_size']
            kind = sym['st_info']['type']
            bind = sym['st_info']['bind']

            kind_str = kind_map.get(kind, kind)
            bind_str = bind_map.get(bind, bind)

            if ignore and name in ignore:
                continue

            if size == 0:
                # not a real object
                continue

            if (names and name not in names) or \
                not _kind_match(select, kind_str) or \
                not _bind_match(select, bind_str):
                # does not fit the choice
                continue
                      
            pois.append(
                PoI(name=name, is_symbol=True, kind=kind_str, 
                    binding=bind_str, addr=addr, size=size)
            )

        return pois

def group_poi_by_checker(pois):
    """ Returns a map {FIC} => {PoI that it checks} """
    pois_per_checker = {}
    for poi in pois:
        if poi.checker_path not in pois_per_checker:
            pois_per_checker[poi.record.checker_path] = []
        pois_per_checker[poi.record.checker_path].append(poi)
    return pois_per_checker

def sanitize_poi_list(pois):
    for poi in pois:
        if poi.record.san_checker_path is None:
            poi.record.san_checker_path = poi.record.checker_path.replace('/', '_')
