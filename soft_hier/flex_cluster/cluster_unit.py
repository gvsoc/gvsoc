#
# Copyright (C) 2020 ETH Zurich and University of Bologna
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

import gvsoc.runner
import pulp.snitch.snitch_core as iss
import memory.memory as memory
import interco.router as router
import gvsoc.systree
from pulp.chips.flex_cluster.cluster_registers import ClusterRegisters
from pulp.chips.flex_cluster.light_redmule import LightRedmule
from pulp.chips.flex_cluster.hwpe_interleaver import HWPEInterleaver
from pulp.chips.flex_cluster.transpose_engine import TransposeEngine
from pulp.chips.flex_cluster.util_dumpper import UtilDumpper
from pulp.snitch.snitch_cluster.dma_interleaver import DmaInterleaver
from pulp.snitch.zero_mem import ZeroMem
from elftools.elf.elffile import *
from pulp.idma.snitch_dma import SnitchDma
from pulp.cluster.l1_interleaver import L1_interleaver
import gvsoc.runner
import math
from pulp.snitch.sequencer import Sequencer
import utils.loader.loader


GAPY_TARGET = True

#Function to get EoC entry
def find_binary_entry(elf_filename):
    # Open the ELF file in binary mode
    with open(elf_filename, 'rb') as f:
        elffile = ELFFile(f)

        # Find the symbol table section in the ELF file
        for section in elffile.iter_sections():
            if isinstance(section, SymbolTableSection):
                # Iterate over symbols in the symbol table
                for symbol in section.iter_symbols():
                    # Check if this symbol's name matches "tohost"
                    if symbol.name == '_start':
                        # Return the symbol's address
                        return symbol['st_value']

    # If the symbol wasn't found, return None
    return None



class Area:

    def __init__(self, base, size):
        self.base = base
        self.size = size



class ClusterArch:
    def __init__(self,  nb_core_per_cluster, base, cluster_id, tcdm_size,
                        stack_base,         stack_size,
                        zomem_base,         zomem_size,
                        reg_base,           reg_size,
                        sync_base,          sync_itlv,          sync_special_mem,
                        insn_base,          insn_size,
                        nb_tcdm_banks,      tcdm_bank_width,
                        redmule_ce_height,  redmule_ce_width,   redmule_ce_pipe,
                        redmule_elem_size,  redmule_queue_depth,
                        redmule_reg_base,   redmule_reg_size,
                        idma_outstand_txn,  idma_outstand_burst,
                        num_cluster_x,      num_cluster_y,
                        spatz_core_list,    spatz_num_vlsu,     spatz_num_fu,
                        spatz_vlsu_bw,      spatz_vreg_gather_eff,
                        data_bandwidth,     auto_fetch=False,   multi_idma_enable=0):

        self.nb_core                = nb_core_per_cluster
        self.base                   = base
        self.cluster_id             = cluster_id
        self.auto_fetch             = auto_fetch
        self.barrier_irq            = 19
        self.tcdm                   = ClusterArch.Tcdm(base, self.nb_core + len(spatz_core_list)*spatz_num_vlsu, tcdm_size, nb_tcdm_banks, tcdm_bank_width, sync_itlv, sync_special_mem)
        self.stack_area             = Area(stack_base, stack_size)
        self.zomem_area             = Area(zomem_base, zomem_size)
        self.sync_area              = Area(sync_base, sync_itlv + sync_special_mem)
        self.reg_area               = Area(reg_base, reg_size)
        self.insn_area              = Area(insn_base, insn_size)

        #Spatz
        self.spatz_core_list        = spatz_core_list
        self.spatz_num_vlsu         = spatz_num_vlsu
        self.spatz_num_fu           = spatz_num_fu
        self.spatz_vlsu_bw          = spatz_vlsu_bw
        self.spatz_vreg_gather_eff  = spatz_vreg_gather_eff

        #RedMule
        self.redmule_ce_height      = redmule_ce_height
        self.redmule_ce_width       = redmule_ce_width
        self.redmule_ce_pipe        = redmule_ce_pipe
        self.redmule_elem_size      = redmule_elem_size
        self.redmule_queue_depth    = redmule_queue_depth
        self.redmule_area           = Area(redmule_reg_base, redmule_reg_size)

        #IDMA
        self.idma_outstand_txn      = idma_outstand_txn
        self.idma_outstand_burst    = idma_outstand_burst
        self.data_bandwidth         = data_bandwidth
        self.multi_idma_enable      = multi_idma_enable

        #Global Information
        self.num_cluster_x          = num_cluster_x
        self.num_cluster_y          = num_cluster_y

    class Tcdm:
        def __init__(self, base, nb_masters, tcdm_size, nb_tcdm_banks, tcdm_bank_width, sync_itlv, sync_special_mem):
            self.area = Area( base, tcdm_size)
            self.nb_tcdm_banks = nb_tcdm_banks
            self.bank_width = tcdm_bank_width
            self.bank_size = (self.area.size / self.nb_tcdm_banks) + self.bank_width #prevent overflow due to RedMule access model
            self.nb_masters = nb_masters
            self.sync_itlv = sync_itlv
            self.sync_special_mem = sync_special_mem


class ClusterTcdm(gvsoc.systree.Component):

    def __init__(self, parent, name, arch):
        super().__init__(parent, name)

        banks = []
        nb_banks = arch.nb_tcdm_banks
        for i in range(0, nb_banks):
            banks.append(memory.Memory(self, f'bank_{i}', size=arch.bank_size, atomics=True, width_log2=int(math.log2(arch.bank_width))))

        interleaver = L1_interleaver(self, 'interleaver', nb_slaves=nb_banks,
            nb_masters=arch.nb_masters, interleaving_bits=int(math.log2(arch.bank_width)))

        dma_interleaver = DmaInterleaver(self, 'dma_interleaver', arch.nb_masters,
            nb_banks, arch.bank_width)

        bus_interleaver = DmaInterleaver(self, 'bus_interleaver', arch.nb_masters,
            nb_banks, arch.bank_width)

        hwpe_interleaver = HWPEInterleaver(self, 'hwpe_interleaver', arch.nb_masters,
            nb_banks, arch.bank_width)

        tcdm_sync_mem = memory.Memory(self, 'sync_mem', size=arch.sync_itlv, atomics=True, width_log2=int(math.log2(arch.bank_width)))

        for i in range(0, nb_banks):
            self.bind(interleaver, 'out_%d' % i, banks[i], 'input')
            self.bind(dma_interleaver, 'out_%d' % i, banks[i], 'input')
            self.bind(bus_interleaver, 'out_%d' % i, banks[i], 'input')
            self.bind(hwpe_interleaver, 'out_%d' % i, banks[i], 'input')

        for i in range(0, arch.nb_masters):
            self.bind(self, f'in_{i}', interleaver, f'in_{i}')
            self.bind(self, f'dma_input', dma_interleaver, f'input')
            self.bind(self, f'bus_input', bus_interleaver, f'input')
            self.bind(self, f'hwpe_input', hwpe_interleaver, f'input')

        self.bind(self, f'sync_input', tcdm_sync_mem, f'input')

    def i_INPUT(self, port: int) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'in_{port}', signature='io')

    def i_DMA_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'dma_input', signature='io')

    def i_BUS_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'bus_input', signature='io')

    def i_HWPE_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'hwpe_input', signature='io')

    def i_SYNC_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'sync_input', signature='io')



class ClusterUnit(gvsoc.systree.Component):

    def __init__(self, parent, name, arch, binary, entry=0, auto_fetch=True):
        super().__init__(parent, name)

        #
        # Components
        #

        #Boot Address
        boot_addr = 0x8000_0000
        if binary is not None:
            boot_addr = find_binary_entry(binary)

        #Loader
        loader = utils.loader.loader.ElfLoader(self, 'loader', binary=binary)

        #Instruction memory
        instr_mem = memory.Memory(self, 'instr_mem', size=arch.insn_area.size, atomics=True, width_log2=-1)

        #Instruction router
        instr_router = router.Router(self, 'instr_router', bandwidth=8*arch.nb_core)

        # Main router
        wide_axi_goto_tcdm = router.Router(self, 'wide_axi_goto_tcdm')
        wide_axi_from_idma = router.Router(self, 'wide_axi_from_idma')
        narrow_axi = router.Router(self, 'narrow_axi', bandwidth=8)

        # L1 Memory
        tcdm = ClusterTcdm(self, 'tcdm', arch.tcdm)

        # Cores
        cores = []
        fp_cores = []
        cores_ico = []
        xfrep = True
        if xfrep:
            fpu_sequencers = []
        for core_id in range(0, arch.nb_core):
            cores.append(iss.Snitch(self, f'pe{core_id}', isa='rv32imfdva',
                fetch_enable=arch.auto_fetch, boot_addr=boot_addr,
                core_id=core_id, htif=False, inc_spatz=(len(arch.spatz_core_list) > 0), spatz_num_vlsu=arch.spatz_num_vlsu, spatz_num_fpu=arch.spatz_num_fu, spatz_vlsu_bw=arch.spatz_vlsu_bw,
                spatz_vreg_gather_eff=arch.spatz_vreg_gather_eff))

            fp_cores.append(iss.Snitch_fp_ss(self, f'fp_ss{core_id}', isa='rv32imfdva',
                fetch_enable=arch.auto_fetch, boot_addr=boot_addr,
                core_id=core_id, htif=False, inc_spatz=(len(arch.spatz_core_list) > 0), spatz_num_vlsu=arch.spatz_num_vlsu, spatz_num_fpu=arch.spatz_num_fu, spatz_vlsu_bw=arch.spatz_vlsu_bw,
                spatz_vreg_gather_eff=arch.spatz_vreg_gather_eff))
            if xfrep:
                fpu_sequencers.append(Sequencer(self, f'fpu_sequencer{core_id}', latency=0))

            cores_ico.append(router.Router(self, f'pe{core_id}_ico', bandwidth=arch.tcdm.bank_width))

        # RedMule
        redmule = LightRedmule(self, f'redmule',
                                    tcdm_bank_width     = arch.tcdm.bank_width,
                                    tcdm_bank_number    = arch.tcdm.nb_tcdm_banks,
                                    elem_size           = arch.redmule_elem_size,
                                    ce_height           = arch.redmule_ce_height,
                                    ce_width            = arch.redmule_ce_width,
                                    ce_pipe             = arch.redmule_ce_pipe,
                                    queue_depth         = arch.redmule_queue_depth)

        # Cluster peripherals
        cluster_registers = ClusterRegisters(self, 'cluster_registers',
            num_cluster_x=arch.num_cluster_x, num_cluster_y=arch.num_cluster_y, nb_cores=arch.nb_core,
            boot_addr=boot_addr, cluster_id=arch.cluster_id, global_barrier_addr=arch.sync_area.base+arch.tcdm.sync_itlv)

        #data dumpper
        data_dumpper = UtilDumpper(self, 'data_dumpper', arch.cluster_id)
        data_dumpper_ctrl_base = arch.reg_area.base + arch.reg_area.size
        data_dumpper_ctrl_size = 64
        data_dumpper_input_base = arch.tcdm.area.base + arch.tcdm.area.size
        data_dumpper_input_size = arch.tcdm.area.size
        ctrl_base_update = arch.reg_area.base + arch.reg_area.size + data_dumpper_ctrl_size


        #Transpose Engine
        transpose_engine = TransposeEngine(self, f'transpose_engine',
                                    tcdm_bank_width     = arch.tcdm.bank_width,
                                    tcdm_bank_number    = arch.tcdm.nb_tcdm_banks,
                                    buffer_dim          = arch.tcdm.nb_tcdm_banks * arch.tcdm.bank_width)
        transpose_engine_ctrl_base = ctrl_base_update
        transpose_engine_ctrl_size = 64
        ctrl_base_update += 64

        # Cluster DMA
        if arch.multi_idma_enable:
            idma_list = []
            for x in range(arch.nb_core):
                idma_list.append(SnitchDma(self, f'idma_{x}', loc_base=arch.tcdm.area.base, loc_size=arch.tcdm.area.size + data_dumpper_input_size,
                tcdm_width=(arch.tcdm.nb_tcdm_banks * arch.tcdm.bank_width), transfer_queue_size=arch.idma_outstand_txn, burst_queue_size=arch.idma_outstand_burst))
                pass
        else:
            idma = SnitchDma(self, 'idma', loc_base=arch.tcdm.area.base, loc_size=arch.tcdm.area.size + data_dumpper_input_size,
                tcdm_width=(arch.tcdm.nb_tcdm_banks * arch.tcdm.bank_width), transfer_queue_size=arch.idma_outstand_txn, burst_queue_size=arch.idma_outstand_burst)
            pass

        #stack memory
        stack_mem = memory.Memory(self, 'stack_mem', size=arch.stack_area.size)

        #zero memory
        zero_mem = ZeroMem(self, 'zero_mem', size=arch.zomem_area.size)

        #synchronization router
        sync_router_master = router.Router(self, 'sync_router_master', bandwidth=4)
        sync_router_slave = router.Router(self, 'sync_router_slave', bandwidth=4)

        #
        # Bindings
        #

        #Binary loader
        loader.o_OUT(instr_router.i_INPUT())
        loader.o_START(cluster_registers.i_INST_PREHEAT_DONE())
        self.o_HBM_PRELOAD_DONE(cluster_registers.i_HBM_PRELOAD_DONE())

        #Instruction router
        instr_router.o_MAP(narrow_axi.i_INPUT())
        instr_router.o_MAP(instr_mem.i_INPUT(), base=arch.insn_area.base, size=arch.insn_area.size, rm_base=True)

        # Narrow router for cores data accesses
        self.o_NARROW_INPUT(narrow_axi.i_INPUT())
        narrow_axi.o_MAP(self.i_NARROW_SOC())
        # TODO check on real HW where this should go. This probably go through wide axi to
        # have good bandwidth when transferring from one cluster to another
        narrow_axi.o_MAP(cores_ico[0].i_INPUT(), base=arch.tcdm.area.base, size=arch.tcdm.area.size, rm_base=False)

        #binding to stack memory
        narrow_axi.o_MAP(stack_mem.i_INPUT(), base=arch.stack_area.base, size=arch.stack_area.size, rm_base=True)

        #binding to cluster registers
        narrow_axi.o_MAP(cluster_registers.i_INPUT(), base=arch.reg_area.base, size=arch.reg_area.size, rm_base=True)

        #binding to data dumpper
        narrow_axi.o_MAP(data_dumpper.i_CTRL(), base=data_dumpper_ctrl_base, size=data_dumpper_ctrl_size, rm_base=True)

        #binding to transpose engine
        narrow_axi.o_MAP(transpose_engine.i_INPUT(), base=transpose_engine_ctrl_base, size=transpose_engine_ctrl_size, rm_base=True)

        #binding to redmule
        narrow_axi.o_MAP(redmule.i_INPUT(), base=arch.redmule_area.base, size=arch.redmule_area.size, rm_base=True)

        #binding back to instruction memory if access needs
        narrow_axi.o_MAP(instr_mem.i_INPUT(), base=arch.insn_area.base, size=arch.insn_area.size, rm_base=True)

        #binding to synchronization bus
        narrow_axi.o_MAP(sync_router_master.i_INPUT(), base=arch.sync_area.base, size=arch.sync_area.size*arch.num_cluster_x*arch.num_cluster_y, rm_base=False)
        sync_router_master.o_MAP(self.i_SYNC_OUTPUT())


        #RedMule to TCDM
        redmule.o_TCDM(tcdm.i_HWPE_INPUT())

        #Transpose Engine to TCDM
        transpose_engine.o_TCDM(tcdm.i_HWPE_INPUT())

        # Wire router for DMA and instruction caches
        self.o_WIDE_INPUT(wide_axi_goto_tcdm.i_INPUT())
        wide_axi_goto_tcdm.o_MAP(tcdm.i_BUS_INPUT())
        wide_axi_from_idma.o_MAP(self.i_WIDE_SOC())
        wide_axi_from_idma.o_MAP(zero_mem.i_INPUT(), base=arch.zomem_area.base, size=arch.zomem_area.size, rm_base=True)


        # iDMA connection
        if arch.multi_idma_enable:
            for x in range(arch.nb_core):
                cores[x].o_OFFLOAD(idma_list[x].i_OFFLOAD())
                idma_list[x].o_OFFLOAD_GRANT(cores[x].i_OFFLOAD_GRANT())
                pass
        else:
            cores[arch.nb_core-1].o_OFFLOAD(idma.i_OFFLOAD())
            idma.o_OFFLOAD_GRANT(cores[arch.nb_core-1].i_OFFLOAD_GRANT())
            pass

        # Cores
        for core_id in range(0, arch.nb_core):
            cluster_registers.o_FETCH_START( cores[core_id].i_FETCHEN() )

        for core_id in range(0, arch.nb_core):
            cores[core_id].o_BARRIER_REQ(cluster_registers.i_BARRIER_ACK(core_id))
        for core_id in range(0, arch.nb_core):
            cores[core_id].o_DATA(cores_ico[core_id].i_INPUT())
            cores_ico[core_id].o_MAP(tcdm.i_INPUT(core_id), base=arch.tcdm.area.base,
                size=arch.tcdm.area.size, rm_base=True)
            cores_ico[core_id].o_MAP(narrow_axi.i_INPUT())
            cores[core_id].o_FETCH(instr_router.i_INPUT())
            cores[core_id].o_REDMULE(redmule.i_CORE_ACC())
            if core_id in arch.spatz_core_list:
                spatz_index = arch.spatz_core_list.index(core_id)
                for vlsu_port in range(arch.spatz_num_vlsu):
                    vlsu_router = router.Router(self, f'spatz_{core_id}_vlsu_{vlsu_port}_router', bandwidth=arch.tcdm.bank_width)
                    vlsu_router.add_mapping("output")
                    self.bind(cores[core_id], f'vlsu_{vlsu_port}', vlsu_router, 'input')
                    self.bind(vlsu_router, 'output', tcdm, f'in_{arch.nb_core + spatz_index*arch.spatz_num_vlsu + vlsu_port}')

        for core_id in range(0, arch.nb_core):
            fp_cores[core_id].o_DATA( cores_ico[core_id].i_INPUT() )
            cluster_registers.o_FETCH_START( fp_cores[core_id].i_FETCHEN() )

            # SSR in fp subsystem datem mover <-> memory port
            self.bind(fp_cores[core_id], 'ssr_dm0', cores_ico[core_id], 'input')
            self.bind(fp_cores[core_id], 'ssr_dm1', cores_ico[core_id], 'input')
            self.bind(fp_cores[core_id], 'ssr_dm2', cores_ico[core_id], 'input')

            # Use WireMaster & WireSlave
            # Add fpu sequence buffer in between int core and fp core to issue instructions
            if xfrep:
                self.bind(cores[core_id], 'acc_req', fpu_sequencers[core_id], 'input')
                self.bind(fpu_sequencers[core_id], 'output', fp_cores[core_id], 'acc_req')
                self.bind(cores[core_id], 'acc_req_ready', fpu_sequencers[core_id], 'acc_req_ready')
                self.bind(fpu_sequencers[core_id], 'acc_req_ready_o', fp_cores[core_id], 'acc_req_ready')
            else:
                # Comment out if we want to add sequencer
                self.bind(cores[core_id], 'acc_req', fp_cores[core_id], 'acc_req')
                self.bind(cores[core_id], 'acc_req_ready', fp_cores[core_id], 'acc_req_ready')

            self.bind(fp_cores[core_id], 'acc_rsp', cores[core_id], 'acc_rsp')

        for core_id in range(0, arch.nb_core):
            self.bind(cluster_registers, f'barrier_ack', cores[core_id], 'barrier_ack')
        for core_id in range(0, arch.nb_core):
            cluster_registers.o_EXTERNAL_IRQ(core_id, cores[core_id].i_IRQ(arch.barrier_irq))

        #Global Synchronization
        self.o_SYNC_INPUT(sync_router_slave.i_INPUT())
        sync_router_slave.o_MAP(tcdm.i_SYNC_INPUT())
        sync_router_slave.o_MAP(cluster_registers.i_GLOBAL_BARRIER_SLAVE(), base=arch.tcdm.sync_itlv, size=arch.tcdm.sync_special_mem, rm_base=True)
        cluster_registers.o_GLOBAL_BARRIER_MASTER(sync_router_master.i_INPUT())

        # Cluster DMA
        data_dumpper_arbiter = router.Router(self, 'data_dumpper_arbiter')
        data_dumpper_arbiter.o_MAP(tcdm.i_DMA_INPUT())
        data_dumpper_arbiter.o_MAP(data_dumpper.i_INPUT(), base=data_dumpper_input_base, size=data_dumpper_input_size, rm_base=True)
        if arch.multi_idma_enable:
            for x in range(arch.nb_core):
                idma_list[x].o_TCDM(data_dumpper_arbiter.i_INPUT())
                idma_list[x].o_AXI(wide_axi_from_idma.i_INPUT())
                pass
        else:
            idma.o_TCDM(data_dumpper_arbiter.i_INPUT())
            idma.o_AXI(wide_axi_from_idma.i_INPUT())
            pass

    def i_WIDE_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'wide_input', signature='io')

    def o_WIDE_INPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('wide_input', itf, signature='io', composite_bind=True)

    def i_WIDE_SOC(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'wide_soc', signature='io')

    def o_WIDE_SOC(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('wide_soc', itf, signature='io')

    def i_NARROW_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'narrow_input', signature='io')

    def o_NARROW_INPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('narrow_input', itf, signature='io', composite_bind=True)

    def i_NARROW_SOC(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'narrow_soc', signature='io')

    def o_NARROW_SOC(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('narrow_soc', itf, signature='io')

    def i_SYNC_OUTPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'sync_output', signature='io')

    def o_SYNC_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('sync_output', itf, signature='io')

    def i_SYNC_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'sync_input', signature='io')

    def o_SYNC_INPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('sync_input', itf, signature='io', composite_bind=True)

    def i_HBM_PRELOAD_DONE(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'hbm_preload_done', signature='wire<bool>')

    def o_HBM_PRELOAD_DONE(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('hbm_preload_done', itf, signature='wire<bool>', composite_bind=True)
