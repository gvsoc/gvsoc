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

# Author: Chi Zhang <chizhang@ethz.ch>
#         Rafael Medina <medinar@iis.ee.ethz.ch>

# Temporary workaround for a DRAMSys bug where Dram.cpp uses the global
# (multi-channel) address as the offset into a per-channel memory buffer,
# causing buffer overflows in StoreMode::Store for channels > 0.
# Fix: one DRAMSys instance per tile, each configured as single-channel
# (hbm2-example.json, 1 channel, 1 GB), with no node-id offset added before
# forwarding to DRAMSys.  Each instance therefore only ever sees addresses in
# [0, hbm_node_addr_space), which fits within the single channel's buffer.

import gvsoc.runner
import cpu.iss.riscv as iss
import memory.memory
import interco.router as router
from vp.clock_domain import Clock_domain
import interco.router as router
import utils.loader.loader
import gvsoc.systree
from pulp.chips.flex_cluster.cluster_unit import ClusterUnit, ClusterArch
from pulp.chips.flex_cluster.ctrl_registers import CtrlRegisters
from pulp.chips.flex_cluster.flex_cluster_arch import FlexClusterArch
from pulp.chips.flex_cluster.flex_mesh_noc import FlexMeshNoC
from pulp.chips.flex_cluster.hbm_ctrl import hbm_ctrl
import memory.dramsys
import math

GAPY_TARGET = True

def is_power_of_two(name, value):
    if value == 0:
        return True
    if value < 0 or (value & (value - 1)) != 0:
        raise AssertionError(f"The value of '{name}' must be a power of two, but got {value}.")
    return True


class FlexClusterDoLMultidsSystem(gvsoc.systree.Component):    # DoL: DRAM on Logic

    def __init__(self, parent, name, parser):
        super().__init__(parent, name)

        #################
        # Configuration #
        #################

        arch            = FlexClusterArch()
        num_clusters    = arch.num_cluster_x * arch.num_cluster_y
        noc_outstanding = (arch.num_cluster_x + arch.num_cluster_y) + arch.noc_outstanding
        num_hbm_ctrl    = arch.num_cluster_x * arch.num_cluster_y

        # Get Binary
        binary = None
        preload_binary = None
        has_preload_binary = 0
        if parser is not None:
            parser.add_argument("--preload", type=str, help="Path to the HBM preload binary file")
            [args, otherArgs] = parser.parse_known_args()
            binary = args.binary
            preload_binary = args.preload
            if preload_binary is not None:
                has_preload_binary = 1

        ######################
        # implicitly setting #
        ######################
        if not hasattr(arch, 'spatz_vlsu_port_width'): arch.spatz_vlsu_port_width = 32
        if not hasattr(arch, 'spatz_vreg_gather_eff'): arch.spatz_vreg_gather_eff = 100
        if not hasattr(arch, 'multi_idma_enable'): arch.multi_idma_enable = 0
        if not hasattr(arch, 'hbm_node_aliase'): arch.hbm_node_aliase = 1
        if not hasattr(arch, 'hbm_node_aliase_start_bit'): arch.hbm_node_aliase_start_bit = 48
        if not hasattr(arch, 'hbm_ctrl_xor_scrambling'): arch.hbm_ctrl_xor_scrambling = 0
        if not hasattr(arch, 'hbm_ctrl_red_scrambling'): arch.hbm_ctrl_red_scrambling = 0
        if not hasattr(arch, 'hbm_type'): arch.hbm_type = 'hbm2'

        #############
        # Assertion #
        #############
        is_power_of_two("arch.num_node_per_ctrl",arch.num_node_per_ctrl)
        is_power_of_two("arch.hbm_chan_placement[0]",arch.hbm_chan_placement[0])
        is_power_of_two("arch.hbm_chan_placement[1]",arch.hbm_chan_placement[1])
        is_power_of_two("arch.hbm_chan_placement[2]",arch.hbm_chan_placement[2])
        is_power_of_two("arch.hbm_chan_placement[3]",arch.hbm_chan_placement[3])
        assert ((arch.num_cluster_x % arch.num_node_per_ctrl) == 0), f"arch.num_node_per_ctrl ({arch.num_node_per_ctrl}) is not suitable for arch.num_cluster_x ({arch.num_cluster_x})"
        assert (arch.num_cluster_x >= arch.num_node_per_ctrl), f"arch.num_node_per_ctrl ({arch.num_node_per_ctrl}) is smaller than arch.num_cluster_x ({arch.num_cluster_x})"
        assert ((arch.num_cluster_y % arch.num_node_per_ctrl) == 0), f"arch.num_node_per_ctrl ({arch.num_node_per_ctrl}) is not suitable for arch.num_cluster_y ({arch.num_cluster_y})"
        assert (arch.num_cluster_y >= arch.num_node_per_ctrl), f"arch.num_node_per_ctrl ({arch.num_node_per_ctrl}) is smaller than arch.num_cluster_y ({arch.num_cluster_y})"
        assert (arch.hbm_node_aliase <= arch.num_node_per_ctrl), f"arch.hbm_node_aliase ({arch.hbm_node_aliase}) is larger than arch.num_node_per_ctrl ({arch.num_node_per_ctrl})"

        ##############
        # Components #
        ##############

        #Clusters
        cluster_list=[]
        for cluster_id in range(num_clusters):
            cluster_arch = ClusterArch( nb_core_per_cluster =   arch.num_core_per_cluster,
                                        base                =   arch.cluster_tcdm_base,
                                        cluster_id          =   cluster_id,
                                        tcdm_size           =   arch.cluster_tcdm_size,
                                        stack_base          =   arch.cluster_stack_base,
                                        stack_size          =   arch.cluster_stack_size,
                                        zomem_base          =   arch.cluster_zomem_base,
                                        zomem_size          =   arch.cluster_zomem_size,
                                        reg_base            =   arch.cluster_reg_base,
                                        reg_size            =   arch.cluster_reg_size,
                                        sync_base           =   arch.sync_base,
                                        sync_itlv           =   arch.sync_interleave,
                                        sync_special_mem    =   arch.sync_special_mem,
                                        insn_base           =   arch.instruction_mem_base,
                                        insn_size           =   arch.instruction_mem_size,
                                        nb_tcdm_banks       =   arch.cluster_tcdm_bank_nb,
                                        tcdm_bank_width     =   arch.cluster_tcdm_bank_width/8,
                                        redmule_ce_height   =   arch.redmule_ce_height,
                                        redmule_ce_width    =   arch.redmule_ce_width,
                                        redmule_ce_pipe     =   arch.redmule_ce_pipe,
                                        redmule_elem_size   =   arch.redmule_elem_size,
                                        redmule_queue_depth =   arch.redmule_queue_depth,
                                        redmule_reg_base    =   arch.redmule_reg_base,
                                        redmule_reg_size    =   arch.redmule_reg_size,
                                        idma_outstand_txn   =   arch.idma_outstand_txn,
                                        idma_outstand_burst =   arch.idma_outstand_burst,
                                        num_cluster_x       =   arch.num_cluster_x,
                                        num_cluster_y       =   arch.num_cluster_y,
                                        spatz_core_list     =   arch.spatz_attaced_core_list,
                                        spatz_num_vlsu      =   arch.spatz_num_vlsu_port,
                                        spatz_num_fu        =   arch.spatz_num_function_unit,
                                        spatz_vlsu_bw       =   arch.spatz_vlsu_port_width,
                                        spatz_vreg_gather_eff = arch.spatz_vreg_gather_eff,
                                        data_bandwidth      =   arch.noc_link_width/8,
                                        multi_idma_enable   =   arch.multi_idma_enable)
            cluster_list.append(ClusterUnit(self,f'cluster_{cluster_id}', cluster_arch, binary))
            pass

        #Virtual router, just for debugging and non-performance-critical jobs (eg, Printf, EoC, Check HBM stored value)
        virtual_interco = router.Router(self, 'virtual_interco', bandwidth=8)

        #Control register
        csr = CtrlRegisters(self, 'ctrl_registers', num_cluster_x=arch.num_cluster_x, num_cluster_y=arch.num_cluster_y, has_preload_binary=has_preload_binary)

        #Synchronization bus
        sync_bus = FlexMeshNoC(self, 'sync_bus', width=4,
                nb_x_clusters=arch.num_cluster_x, nb_y_clusters=arch.num_cluster_y,
                ni_outstanding_reqs=noc_outstanding, router_input_queue_size=noc_outstanding * num_clusters, atomics=1, collective=1)

        # One DRAMSys instance per tile, each single-channel (hbm2 = 1 channel, 1 GB).
        # The hbm_itf router forwards each tile's local address range [0, hbm_node_addr_space)
        # without adding a node-id offset, so each DRAMSys instance only ever sees
        # addresses within [0, hbm_node_addr_space) — safely within the 1 GB buffer.
        hbm_chan_list = []
        for node_id in range(num_hbm_ctrl):
            hbm_chan_list.append(memory.dramsys.Dramsys(self, f'hbm_chan_{node_id}', dram_type='hbm2'))
            pass

        #HBM controllers — one per tile, connected to its own DRAMSys instance
        hbm_ctrl_list = []
        hbm_router_list = []
        for node_id in range(num_hbm_ctrl):
            hbm_ctrl_list.append(hbm_ctrl(self, f'hbm_ctrl_{node_id}', nb_slaves=1, nb_masters=1, interleaving_bits=int(math.log2(arch.noc_link_width/8)), node_addr_offset=arch.hbm_node_addr_space, hbm_node_aliase=arch.hbm_node_aliase, xor_scrambling=arch.hbm_ctrl_xor_scrambling, red_scrambling=arch.hbm_ctrl_red_scrambling))
            hbm_router_list.append(router.Router(self, f'hbm_itf_{node_id}'))
            # No add_offset here: the router strips the tile's base address so that
            # DRAMSys receives a local address in [0, hbm_node_addr_space).
            hbm_router_list[node_id].add_mapping('output')
            self.bind(hbm_router_list[node_id], 'output', hbm_ctrl_list[node_id], f'in_0')
            self.bind(hbm_ctrl_list[node_id], 'out_0', hbm_chan_list[node_id], f'input')
            pass

        #NoC
        data_noc = FlexMeshNoC(self, 'data_noc', width=arch.noc_link_width/8,
                nb_x_clusters=arch.num_cluster_x, nb_y_clusters=arch.num_cluster_y,
                ni_outstanding_reqs=noc_outstanding, router_input_queue_size=noc_outstanding * num_clusters, collective=1,
                edge_node_alias=arch.hbm_node_aliase, edge_node_alias_start_bit=arch.hbm_node_aliase_start_bit)


        #Debug Memory
        debug_mem = memory.memory.Memory(self,'debug_mem', size=1)

        #HBM Preloader
        hbm_preloader = utils.loader.loader.ElfLoader(self, 'hbm_preloader', binary=preload_binary)

        ############
        # Bindings #
        ############

        #Debug memory
        virtual_interco.o_MAP(debug_mem.i_INPUT())
        virtual_interco.o_MAP(data_noc.i_CLUSTER_INPUT(0, 0), base=arch.hbm_start_base, size=arch.hbm_node_addr_space * 2 * (arch.num_cluster_x + arch.num_cluster_y), rm_base=False)

        #Control register
        virtual_interco.o_MAP(csr.i_INPUT(), base=arch.soc_register_base, size=arch.soc_register_size, rm_base=True)

        #HBM Preloader
        hbm_preloader.o_OUT(data_noc.i_CLUSTER_INPUT(0, 0))
        hbm_preloader.o_START(csr.i_HBM_PRELOAD_DONE())

        #Clusters   @NOTE: additional router to allow o_WIDE_SOC to go either to local hbm interface or to NoC
        cluster_out_router_list = []
        for cluster_id in range(num_clusters):
            cluster_list[cluster_id].o_NARROW_SOC(virtual_interco.i_INPUT())
            csr.o_HBM_PRELOAD_DONE_TO_CLUSTER(cluster_list[cluster_id].i_HBM_PRELOAD_DONE(),cluster_id)
            cluster_out_router_list.append(router.Router(self, f'cluster_out_itf_{cluster_id}'))
            pass

        #Data NoC + Sync NoC
        for node_id in range(num_clusters):
            x_id = int(node_id%arch.num_cluster_x)
            y_id = int(node_id/arch.num_cluster_x)
            hbm_node_size = arch.hbm_node_addr_space
            hbm_base_addr = arch.hbm_start_base + node_id * hbm_node_size
            cluster_list[node_id].o_WIDE_SOC(cluster_out_router_list[node_id].i_INPUT())
            cluster_out_router_list[node_id].o_MAP(data_noc.i_CLUSTER_INPUT(x_id, y_id))
            cluster_out_router_list[node_id].o_MAP(hbm_router_list[node_id].i_INPUT(), base=hbm_base_addr, size=hbm_node_size)
            cluster_list[node_id].o_SYNC_OUTPUT(sync_bus.i_CLUSTER_INPUT(x_id, y_id))
            data_noc.o_MAP(cluster_list[node_id].i_WIDE_INPUT(), base=arch.cluster_tcdm_remote  + node_id*arch.cluster_tcdm_size,   size=arch.cluster_tcdm_size,    x=x_id+1, y=y_id+1)
            data_noc.o_MAP(hbm_router_list[node_id].i_INPUT(), base=hbm_base_addr, size=hbm_node_size, x=x_id+1, y=y_id+1)
            sync_bus.o_MAP(cluster_list[node_id].i_SYNC_INPUT(), base=arch.sync_base            + node_id*(arch.sync_interleave + arch.sync_special_mem),     size=(arch.sync_interleave + arch.sync_special_mem),      x=x_id+1, y=y_id+1)
            pass



class FlexClusterDoLMultidsBoard(gvsoc.systree.Component):

    def __init__(self, parent, name, parser, options):
        super(FlexClusterDoLMultidsBoard, self).__init__(parent, name, options=options)

        arch  = FlexClusterArch()
        clock = Clock_domain(self, 'clock', frequency=(1000000000 if not hasattr(arch, 'frequence') else arch.frequence))

        flex_cluster_system = FlexClusterDoLMultidsSystem(self, 'chip', parser)

        self.bind(clock, 'out', flex_cluster_system, 'clock')

class Target(gvsoc.runner.Target):

    def __init__(self, parser, options):
        super(Target, self).__init__(parser, options,
            model=FlexClusterDoLMultidsBoard, description="Flex Cluster DoL virtual board (single-channel DRAMSys workaround)")
