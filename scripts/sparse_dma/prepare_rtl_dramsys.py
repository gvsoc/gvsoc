#!/usr/bin/env python3
"""Copy the reproduced RTL DRAMSys sources and adapt only its C entry point."""
from pathlib import Path
import difflib
import shutil

repo = Path(__file__).resolve().parents[2]
checkouts = repo.parent / 'spatz-sparse-dma/.bender/git/checkouts'
sources = list(checkouts.glob('dram_rtl_sim-*/dramsys_lib/DRAMSys'))
if len(sources) != 1:
    raise RuntimeError(f'Expected one initialized RTL DRAMSys checkout, found {sources}')
source = sources[0]
out = repo / 'build/sparse_dma'
dest = out / 'dramsys-rtl'
if not dest.exists():
    shutil.copytree(source, dest, ignore=shutil.ignore_patterns(
        '.git', 'build', '__pycache__', '*.tdb', '*.vcd'))
relative = Path('src/simulator/simulator/dramsys_lib.cpp')
original = (source / relative).read_text()
adapted = original.replace(
    'extern "C" int add_dram(char * resources_path, char * simulationJson_path, uint64_t dram_base) {',
    '''struct GvsocMemspec {
    unsigned access_size, nb_channels, nb_pseudo_channels, nb_ranks;
    unsigned nb_bank_groups, nb_banks, nb_rows, nb_columns;
    unsigned channel_stride, rank_stride, bankgroup_stride, bank_stride, row_stride, column_stride;
};

extern "C" int add_dram(char * resources_path, char * simulationJson_path, GvsocMemspec *memspec) {
    const uint64_t dram_base = 0; // GVSoC routes DRAM-relative addresses.
''')
if adapted == original:
    raise RuntimeError('RTL add_dram signature changed; review the adapter')
marker = '    uint64_t dramMaxBurstByte = config.memSpec->maxBytesPerBurst;'
adapted = adapted.replace(marker, marker + '''

    if (memspec) {
        const auto &spec = *config.memSpec;
        const auto &decoder = dramSys->getAddressDecoder();
        *memspec = {};
        memspec->access_size = dramMaxBurstByte;
        memspec->nb_channels = spec.numberOfChannels;
        memspec->nb_pseudo_channels = spec.pseudoChannelsPerChannel;
        memspec->nb_ranks = spec.ranksPerChannel;
        memspec->nb_bank_groups = spec.groupsPerRank;
        memspec->nb_banks = spec.banksPerGroup;
        memspec->nb_rows = spec.rowsPerBank;
        memspec->nb_columns = spec.columnsPerRow;
        memspec->channel_stride = spec.numberOfChannels > 1
            ? decoder.encodeAddress({1,0,0,0,0,0,0}) : 0;
        memspec->rank_stride = decoder.encodeAddress({0,1,0,0,0,0,0});
        memspec->bankgroup_stride = decoder.encodeAddress({0,0,1,0,0,0,0});
        memspec->bank_stride = decoder.encodeAddress({0,0,0,1,0,0,0});
        memspec->row_stride = decoder.encodeAddress({0,0,0,0,1,0,0});
        memspec->column_stride = decoder.encodeAddress({0,0,0,0,0,1,0});
    }
''')
(dest / relative).write_text(adapted)
(out / 'dramsys-api-adapter.patch').write_text(''.join(difflib.unified_diff(
    original.splitlines(True), adapted.splitlines(True),
    fromfile='a/' + str(relative), tofile='b/' + str(relative))))
(out / 'rtl-dramsys-source.txt').write_text(str(source) + '\n')
print('Prepared RTL DRAMSys sources and GVSoC add_dram API adapter.')
