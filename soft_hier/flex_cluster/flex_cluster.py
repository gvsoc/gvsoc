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
from pulp.cluster.l1_interleaver import L1_interleaver
import memory.dramsys
import math

GAPY_TARGET = True


class FlexClusterSystem(gvsoc.systree.Component):

    def __init__(self, parent, name, parser):
        super().__init__(parent, name)

        #################
        # Configuration #
        #################

        arch            = FlexClusterArch()
        num_clusters    = arch.num_cluster_x * arch.num_cluster_y
        noc_outstanding = (arch.num_cluster_x + arch.num_cluster_y) + arch.noc_outstanding

        # Get Binary
        binary = None
        if parser is not None:
            [args, otherArgs] = parser.parse_known_args()
            binary = args.binary

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
                                        reg_base            =   arch.cluster_reg_base,
                                        reg_size            =   arch.cluster_reg_size,
                                        sync_base           =   arch.sync_base,
                                        sync_size           =   arch.num_cluster_x*arch.num_cluster_y*arch.sync_interleave,
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
                                        data_bandwidth      =   arch.noc_link_width/8)
            cluster_list.append(ClusterUnit(self,f'cluster_{cluster_id}', cluster_arch, binary))
            pass

        #Narrow AXI router
        narrow_interco = router.Router(self, 'narrow_interco', bandwidth=8)

        #Control register
        csr = CtrlRegisters(self, 'ctrl_registers', num_cluster_x=arch.num_cluster_x, num_cluster_y=arch.num_cluster_y)

        #Synchronization bus
        sync_bus = FlexMeshNoC(self, 'sync_bus', width=4,
                nb_x_clusters=arch.num_cluster_x, nb_y_clusters=arch.num_cluster_y,
                ni_outstanding_reqs=noc_outstanding, router_input_queue_size=noc_outstanding * num_clusters, atomics=1)

        #HBM channels
        hbm_list_west = []
        for hbm_ch in range(arch.hbm_placement[0] * arch.num_hbm_ch_per_node):
            hbm_list_west.append(memory.dramsys.Dramsys(self, f'west_hbm_chan_{hbm_ch}'))
            pass

        hbm_list_north = []
        for hbm_ch in range(arch.hbm_placement[1] * arch.num_hbm_ch_per_node):
            hbm_list_north.append(memory.dramsys.Dramsys(self, f'north_hbm_chan_{hbm_ch}'))
            pass

        hbm_list_east = []
        for hbm_ch in range(arch.hbm_placement[2] * arch.num_hbm_ch_per_node):
            hbm_list_east.append(memory.dramsys.Dramsys(self, f'east_hbm_chan_{hbm_ch}'))
            pass

        hbm_list_south = []
        for hbm_ch in range(arch.hbm_placement[3] * arch.num_hbm_ch_per_node):
            hbm_list_south.append(memory.dramsys.Dramsys(self, f'south_hbm_chan_{hbm_ch}'))
            pass

        #NoC
        data_noc = FlexMeshNoC(self, 'data_noc', width=arch.noc_link_width/8,
                nb_x_clusters=arch.num_cluster_x, nb_y_clusters=arch.num_cluster_y,
                ni_outstanding_reqs=noc_outstanding, router_input_queue_size=noc_outstanding * num_clusters)


        #Debug Memory
        debug_mem = memory.memory.Memory(self,'debug_mem', size=1)

        ############
        # Bindings #
        ############

        #Debug memory
        narrow_interco.o_MAP(debug_mem.i_INPUT())
        narrow_interco.o_MAP(data_noc.i_CLUSTER_INPUT(0, 0), base=arch.hbm_start_base, size=arch.hbm_node_addr_space * (arch.hbm_placement[0] + arch.hbm_placement[1] + arch.hbm_placement[2] + arch.hbm_placement[3]), rm_base=False)

        #Control register
        narrow_interco.o_MAP(csr.i_INPUT(), base=arch.soc_register_base, size=arch.soc_register_size, rm_base=True)
        for cluster_id in range(num_clusters):
            csr.o_BARRIER_ACK(cluster_list[cluster_id].i_SYNC_IRQ())
            pass

        #Clusters
        for cluster_id in range(num_clusters):
            cluster_list[cluster_id].o_NARROW_SOC(narrow_interco.i_INPUT())
            pass

        #Data NoC + Sync NoC
        for node_id in range(num_clusters):
            x_id = int(node_id%arch.num_cluster_x)
            y_id = int(node_id/arch.num_cluster_x)
            cluster_list[node_id].o_WIDE_SOC(data_noc.i_CLUSTER_INPUT(x_id, y_id))
            cluster_list[node_id].o_SYNC_OUTPUT(sync_bus.i_CLUSTER_INPUT(x_id, y_id))
            data_noc.o_MAP(cluster_list[node_id].i_WIDE_INPUT(), base=arch.cluster_tcdm_remote  + node_id*arch.cluster_tcdm_size,   size=arch.cluster_tcdm_size,    x=x_id+1, y=y_id+1)
            sync_bus.o_MAP(cluster_list[node_id].i_SYNC_INPUT(), base=arch.sync_base            + node_id*arch.sync_interleave,     size=arch.sync_interleave,      x=x_id+1, y=y_id+1)
            pass

        #HBM connection
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
            itf_router = router.Router(self, f'west_{node_id}')
            itf_router.add_mapping('output')
            if arch.hbm_placement[0] != 0:
                hbm_interlever = L1_interleaver(self, f'west_hbm_interleaver_{node_id}', nb_slaves=arch.num_hbm_ch_per_node, nb_masters=1, interleaving_bits=int(math.log2(arch.noc_link_width/8)))
                self.bind(itf_router, 'output', hbm_interlever, 'in_0')
                for sub_hbm in range(arch.num_hbm_ch_per_node):
                    self.bind(hbm_interlever, f'out_{sub_hbm}', hbm_list_west[int(node_id%(arch.hbm_placement[0]*arch.num_hbm_ch_per_node))*arch.num_hbm_ch_per_node + sub_hbm], 'input')
                    pass
            data_noc.o_MAP(itf_router.i_INPUT(), base=hbm_edge_start_base+node_id*arch.hbm_node_addr_space, size=arch.hbm_node_addr_space, x=0, y=node_id+1)
            pass
        hbm_edge_start_base += arch.num_cluster_y*arch.hbm_node_addr_space

        ## north
        for node_id in range(arch.num_cluster_x):
            itf_router = router.Router(self, f'north_{node_id}')
            itf_router.add_mapping('output')
            if arch.hbm_placement[1] != 0:
                hbm_interlever = L1_interleaver(self, f'north_hbm_interleaver_{node_id}', nb_slaves=arch.num_hbm_ch_per_node, nb_masters=1, interleaving_bits=int(math.log2(arch.noc_link_width/8)))
                self.bind(itf_router, 'output', hbm_interlever, 'in_0')
                for sub_hbm in range(arch.num_hbm_ch_per_node):
                    self.bind(hbm_interlever, f'out_{sub_hbm}', hbm_list_north[int(node_id%(arch.hbm_placement[1]*arch.num_hbm_ch_per_node))*arch.num_hbm_ch_per_node + sub_hbm], 'input')
                    pass
            data_noc.o_MAP(itf_router.i_INPUT(), base=hbm_edge_start_base+node_id*arch.hbm_node_addr_space, size=arch.hbm_node_addr_space, x=node_id+1, y=arch.num_cluster_y+1)
            pass
        hbm_edge_start_base += arch.num_cluster_x*arch.hbm_node_addr_space

        ## east
        for node_id in range(arch.num_cluster_y):
            itf_router = router.Router(self, f'east_{node_id}')
            itf_router.add_mapping('output')
            if arch.hbm_placement[2] != 0:
                hbm_interlever = L1_interleaver(self, f'east_hbm_interleaver_{node_id}', nb_slaves=arch.num_hbm_ch_per_node, nb_masters=1, interleaving_bits=int(math.log2(arch.noc_link_width/8)))
                self.bind(itf_router, 'output', hbm_interlever, 'in_0')
                for sub_hbm in range(arch.num_hbm_ch_per_node):
                    self.bind(hbm_interlever, f'out_{sub_hbm}', hbm_list_east[int(node_id%(arch.hbm_placement[2]*arch.num_hbm_ch_per_node))*arch.num_hbm_ch_per_node + sub_hbm], 'input')
                    pass
            data_noc.o_MAP(itf_router.i_INPUT(), base=hbm_edge_start_base+node_id*arch.hbm_node_addr_space, size=arch.hbm_node_addr_space, x=arch.num_cluster_x+1, y=node_id+1)
            pass
        hbm_edge_start_base += arch.num_cluster_y*arch.hbm_node_addr_space

        ## south
        for node_id in range(arch.num_cluster_x):
            itf_router = router.Router(self, f'south_{node_id}')
            itf_router.add_mapping('output')
            if arch.hbm_placement[3] != 0:
                hbm_interlever = L1_interleaver(self, f'south_hbm_interleaver_{node_id}', nb_slaves=arch.num_hbm_ch_per_node, nb_masters=1, interleaving_bits=int(math.log2(arch.noc_link_width/8)))
                self.bind(itf_router, 'output', hbm_interlever, 'in_0')
                for sub_hbm in range(arch.num_hbm_ch_per_node):
                    self.bind(hbm_interlever, f'out_{sub_hbm}', hbm_list_south[int(node_id%(arch.hbm_placement[3]*arch.num_hbm_ch_per_node))*arch.num_hbm_ch_per_node + sub_hbm], 'input')
                    pass
            data_noc.o_MAP(itf_router.i_INPUT(), base=hbm_edge_start_base+node_id*arch.hbm_node_addr_space, size=arch.hbm_node_addr_space, x=node_id+1, y=0)
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