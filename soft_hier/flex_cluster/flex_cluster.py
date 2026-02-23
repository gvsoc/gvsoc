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


class FlexClusterSystem(gvsoc.systree.Component):

    def __init__(self, parent, name, parser):
        super().__init__(parent, name)

        #################
        # Configuration #
        #################

        arch            = FlexClusterArch()
        num_clusters    = arch.num_cluster_x * arch.num_cluster_y
        noc_outstanding = (arch.num_cluster_x + arch.num_cluster_y) + arch.noc_outstanding
        num_hbm_ctrl_x  = arch.num_cluster_x // arch.num_node_per_ctrl
        num_hbm_ctrl_y  = arch.num_cluster_y // arch.num_node_per_ctrl

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
        assert ((arch.hbm_chan_placement[0] >= num_hbm_ctrl_y) or (arch.hbm_chan_placement[0] == 0)), f"arch.hbm_chan_placement[0] ({arch.hbm_chan_placement[0]}) is smaller than hbm controller on y direction ({num_hbm_ctrl_y})"
        assert ((arch.hbm_chan_placement[1] >= num_hbm_ctrl_x) or (arch.hbm_chan_placement[1] == 0)), f"arch.hbm_chan_placement[1] ({arch.hbm_chan_placement[1]}) is smaller than hbm controller on x direction ({num_hbm_ctrl_x})"
        assert ((arch.hbm_chan_placement[2] >= num_hbm_ctrl_y) or (arch.hbm_chan_placement[2] == 0)), f"arch.hbm_chan_placement[2] ({arch.hbm_chan_placement[2]}) is smaller than hbm controller on y direction ({num_hbm_ctrl_y})"
        assert ((arch.hbm_chan_placement[3] >= num_hbm_ctrl_x) or (arch.hbm_chan_placement[3] == 0)), f"arch.hbm_chan_placement[3] ({arch.hbm_chan_placement[3]}) is smaller than hbm controller on x direction ({num_hbm_ctrl_x})"
        assert (arch.hbm_node_aliase <= arch.num_node_per_ctrl), f"arch.hbm_node_aliase ({arch.hbm_node_aliase}) is larger than arch.num_node_per_ctrl ({arch.num_node_per_ctrl})"

        ctrl_chan_west  = arch.hbm_chan_placement[0] // num_hbm_ctrl_y
        ctrl_chan_north = arch.hbm_chan_placement[1] // num_hbm_ctrl_x
        ctrl_chan_east  = arch.hbm_chan_placement[2] // num_hbm_ctrl_y
        ctrl_chan_south = arch.hbm_chan_placement[3] // num_hbm_ctrl_x

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

        #HBM channels
        hbm_chan_list_west = []
        for hbm_ch in range(arch.hbm_chan_placement[0]):
            hbm_chan_list_west.append(memory.dramsys.Dramsys(self, f'west_hbm_chan_{hbm_ch}'))
            pass

        hbm_chan_list_north = []
        for hbm_ch in range(arch.hbm_chan_placement[1]):
            hbm_chan_list_north.append(memory.dramsys.Dramsys(self, f'north_hbm_chan_{hbm_ch}'))
            pass

        hbm_chan_list_east = []
        for hbm_ch in range(arch.hbm_chan_placement[2]):
            hbm_chan_list_east.append(memory.dramsys.Dramsys(self, f'east_hbm_chan_{hbm_ch}'))
            pass

        hbm_chan_list_south = []
        for hbm_ch in range(arch.hbm_chan_placement[3]):
            hbm_chan_list_south.append(memory.dramsys.Dramsys(self, f'south_hbm_chan_{hbm_ch}'))
            pass

        #HBM controllers
        hbm_ctrl_list_west = []
        for hbm_ct in range(num_hbm_ctrl_y):
            nb_slaves=ctrl_chan_west
            hbm_ctrl_list_west.append(hbm_ctrl(self, f'west_hbm_ctrl_{hbm_ct}', nb_slaves=nb_slaves, nb_masters=arch.num_node_per_ctrl, interleaving_bits=int(math.log2(arch.noc_link_width/8)), node_addr_offset=arch.hbm_node_addr_space, hbm_node_aliase=arch.hbm_node_aliase, xor_scrambling=arch.hbm_ctrl_xor_scrambling, red_scrambling=arch.hbm_ctrl_red_scrambling))
            pass

        hbm_ctrl_list_north = []
        for hbm_ct in range(num_hbm_ctrl_x):
            nb_slaves=ctrl_chan_north
            hbm_ctrl_list_north.append(hbm_ctrl(self, f'north_hbm_ctrl_{hbm_ct}', nb_slaves=nb_slaves, nb_masters=arch.num_node_per_ctrl, interleaving_bits=int(math.log2(arch.noc_link_width/8)), node_addr_offset=arch.hbm_node_addr_space, hbm_node_aliase=arch.hbm_node_aliase, xor_scrambling=arch.hbm_ctrl_xor_scrambling, red_scrambling=arch.hbm_ctrl_red_scrambling))
            pass

        hbm_ctrl_list_east = []
        for hbm_ct in range(num_hbm_ctrl_y):
            nb_slaves=ctrl_chan_east
            hbm_ctrl_list_east.append(hbm_ctrl(self, f'east_hbm_ctrl_{hbm_ct}', nb_slaves=nb_slaves, nb_masters=arch.num_node_per_ctrl, interleaving_bits=int(math.log2(arch.noc_link_width/8)), node_addr_offset=arch.hbm_node_addr_space, hbm_node_aliase=arch.hbm_node_aliase, xor_scrambling=arch.hbm_ctrl_xor_scrambling, red_scrambling=arch.hbm_ctrl_red_scrambling))
            pass

        hbm_ctrl_list_south = []
        for hbm_ct in range(num_hbm_ctrl_x):
            nb_slaves=ctrl_chan_south
            hbm_ctrl_list_south.append(hbm_ctrl(self, f'south_hbm_ctrl_{hbm_ct}', nb_slaves=nb_slaves, nb_masters=arch.num_node_per_ctrl, interleaving_bits=int(math.log2(arch.noc_link_width/8)), node_addr_offset=arch.hbm_node_addr_space, hbm_node_aliase=arch.hbm_node_aliase, xor_scrambling=arch.hbm_ctrl_xor_scrambling, red_scrambling=arch.hbm_ctrl_red_scrambling))
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

        #Clusters
        for cluster_id in range(num_clusters):
            cluster_list[cluster_id].o_NARROW_SOC(virtual_interco.i_INPUT())
            csr.o_HBM_PRELOAD_DONE_TO_CLUSTER(cluster_list[cluster_id].i_HBM_PRELOAD_DONE(),cluster_id)
            pass

        #Data NoC + Sync NoC
        for node_id in range(num_clusters):
            x_id = int(node_id%arch.num_cluster_x)
            y_id = int(node_id/arch.num_cluster_x)
            cluster_list[node_id].o_WIDE_SOC(data_noc.i_CLUSTER_INPUT(x_id, y_id))
            cluster_list[node_id].o_SYNC_OUTPUT(sync_bus.i_CLUSTER_INPUT(x_id, y_id))
            data_noc.o_MAP(cluster_list[node_id].i_WIDE_INPUT(), base=arch.cluster_tcdm_remote  + node_id*arch.cluster_tcdm_size,   size=arch.cluster_tcdm_size,    x=x_id+1, y=y_id+1)
            sync_bus.o_MAP(cluster_list[node_id].i_SYNC_INPUT(), base=arch.sync_base            + node_id*(arch.sync_interleave + arch.sync_special_mem),     size=(arch.sync_interleave + arch.sync_special_mem),      x=x_id+1, y=y_id+1)
            pass

        #HBM controllers and channels connections
        #Mapping:
        #         1->
        #      ________
        #   ^ |        | ^
        #   | |        | |
        #   0 |        | 2
        #     |________|
        #
        #         3->
        hbm_edge_start_base = arch.hbm_start_base

        ## west
        for node_id in range(arch.num_cluster_y):
            ctrl_id = node_id // arch.num_node_per_ctrl
            itf_router = router.Router(self, f'west_{node_id}')
            itf_router.add_mapping('output')
            aliase_offset = (1 << arch.hbm_node_aliase_start_bit)
            base_addr = hbm_edge_start_base + (node_id // arch.hbm_node_aliase) * (arch.hbm_node_addr_space * arch.hbm_node_aliase) + aliase_offset * (node_id % arch.hbm_node_aliase)
            node_size = arch.hbm_node_addr_space * arch.hbm_node_aliase
            data_noc.o_MAP(itf_router.i_INPUT(), base=base_addr, size=node_size, x=0, y=node_id+1)
            self.bind(itf_router, 'output', hbm_ctrl_list_west[ctrl_id], f'in_{node_id % arch.num_node_per_ctrl}')
            pass
        for chan_id in range(arch.hbm_chan_placement[0]):
            ctrl_id = chan_id // ctrl_chan_west
            self.bind(hbm_ctrl_list_west[ctrl_id], f'out_{chan_id % ctrl_chan_west}', hbm_chan_list_west[chan_id], 'input')
            pass
        hbm_edge_start_base += arch.num_cluster_y*arch.hbm_node_addr_space

        ## north
        for node_id in range(arch.num_cluster_x):
            ctrl_id = node_id // arch.num_node_per_ctrl
            itf_router = router.Router(self, f'north_{node_id}')
            itf_router.add_mapping('output')
            aliase_offset = (1 << arch.hbm_node_aliase_start_bit)
            base_addr = hbm_edge_start_base + (node_id // arch.hbm_node_aliase) * (arch.hbm_node_addr_space * arch.hbm_node_aliase) + aliase_offset * (node_id % arch.hbm_node_aliase)
            node_size = arch.hbm_node_addr_space * arch.hbm_node_aliase
            data_noc.o_MAP(itf_router.i_INPUT(), base=base_addr, size=node_size, x=node_id+1, y=arch.num_cluster_y+1)
            self.bind(itf_router, 'output', hbm_ctrl_list_north[ctrl_id], f'in_{node_id % arch.num_node_per_ctrl}')
            pass
        for chan_id in range(arch.hbm_chan_placement[1]):
            ctrl_id = chan_id // ctrl_chan_north
            self.bind(hbm_ctrl_list_north[ctrl_id], f'out_{chan_id % ctrl_chan_north}', hbm_chan_list_north[chan_id], 'input')
            pass
        hbm_edge_start_base += arch.num_cluster_x*arch.hbm_node_addr_space

        ## east
        for node_id in range(arch.num_cluster_y):
            ctrl_id = node_id // arch.num_node_per_ctrl
            itf_router = router.Router(self, f'east_{node_id}')
            itf_router.add_mapping('output')
            aliase_offset = (1 << arch.hbm_node_aliase_start_bit)
            base_addr = hbm_edge_start_base + (node_id // arch.hbm_node_aliase) * (arch.hbm_node_addr_space * arch.hbm_node_aliase) + aliase_offset * (node_id % arch.hbm_node_aliase)
            node_size = arch.hbm_node_addr_space * arch.hbm_node_aliase
            data_noc.o_MAP(itf_router.i_INPUT(), base=base_addr, size=node_size, x=arch.num_cluster_x+1, y=node_id+1)
            self.bind(itf_router, 'output', hbm_ctrl_list_east[ctrl_id], f'in_{node_id % arch.num_node_per_ctrl}')
            pass
        for chan_id in range(arch.hbm_chan_placement[2]):
            ctrl_id = chan_id // ctrl_chan_east
            self.bind(hbm_ctrl_list_east[ctrl_id], f'out_{chan_id % ctrl_chan_east}', hbm_chan_list_east[chan_id], 'input')
            pass
        hbm_edge_start_base += arch.num_cluster_y*arch.hbm_node_addr_space

        ## south
        for node_id in range(arch.num_cluster_x):
            ctrl_id = node_id // arch.num_node_per_ctrl
            itf_router = router.Router(self, f'south_{node_id}')
            itf_router.add_mapping('output')
            aliase_offset = (1 << arch.hbm_node_aliase_start_bit)
            base_addr = hbm_edge_start_base + (node_id // arch.hbm_node_aliase) * (arch.hbm_node_addr_space * arch.hbm_node_aliase) + aliase_offset * (node_id % arch.hbm_node_aliase)
            node_size = arch.hbm_node_addr_space * arch.hbm_node_aliase
            data_noc.o_MAP(itf_router.i_INPUT(), base=base_addr, size=node_size, x=node_id+1, y=0)
            self.bind(itf_router, 'output', hbm_ctrl_list_south[ctrl_id], f'in_{node_id % arch.num_node_per_ctrl}')
            pass
        for chan_id in range(arch.hbm_chan_placement[3]):
            ctrl_id = chan_id // ctrl_chan_south
            self.bind(hbm_ctrl_list_south[ctrl_id], f'out_{chan_id % ctrl_chan_south}', hbm_chan_list_south[chan_id], 'input')
            pass



class FlexClusterBoard(gvsoc.systree.Component):

    def __init__(self, parent, name, parser, options):
        super(FlexClusterBoard, self).__init__(parent, name, options=options)

        clock = Clock_domain(self, 'clock', frequency=1000000000)

        flex_cluster_system = FlexClusterSystem(self, 'chip', parser)

        self.bind(clock, 'out', flex_cluster_system, 'clock')

class Target(gvsoc.runner.Target):

    def __init__(self, parser, options):
        super(Target, self).__init__(parser, options,
            model=FlexClusterBoard, description="Flex Cluster virtual board")