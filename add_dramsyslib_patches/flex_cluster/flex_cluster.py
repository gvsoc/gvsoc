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
import pulp.floonoc.floonoc

GAPY_TARGET = True


class FlexClusterSystem(gvsoc.systree.Component):

    def __init__(self, parent, name, parser):
        super().__init__(parent, name)

        #################
        # Configuration #
        #################

        arch            = FlexClusterArch()
        num_clusters    = arch.num_cluster_x * arch.num_cluster_y

        # Get Binary
        binary = None
        if parser is not None:
            [args, otherArgs] = parser.parse_known_args()
            binary = args.binary

        ##############
        # Components #
        ##############

        #Loader
        loader = utils.loader.loader.ElfLoader(self, 'loader', binary=binary)

        #Instruction memory
        instr_mem = memory.memory.Memory(self, 'instr_mem', size=arch.instruction_mem_size, atomics=True)

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
                                        sync_size           =   arch.num_cluster_x*arch.num_cluster_y*arch.sync_interleave)
            cluster_list.append(ClusterUnit(self,f'cluster_{cluster_id}', cluster_arch))
            pass

        #Instruction router
        instr_router = router.Router(self, 'instr_router')

        #Control register
        csr = CtrlRegisters(self, 'ctrl_registers')

        #Synchronization bus
        sync_bus = router.Router(self, 'sync_bus', bandwidth=4)

        #Synchronization memory
        sync_mem_list = []
        for cluster_id in range(num_clusters):
            sync_mem_list.append(memory.memory.Memory(self, f'sync_mem{cluster_id}', size=arch.sync_interleave, atomics=True))
            pass

        #HBM channels
        hbm_list = []
        for hbm_ch in range(num_clusters):
            hbm_list.append(memory.memory.Memory(self, f'hbm_ch{hbm_ch}', size=arch.cluster_tcdm_size))
            pass

        #Noc
        noc = pulp.floonoc.floonoc.FlooNocClusterGrid(self, 'noc', width=arch.noc_link_width/8,
                nb_x_clusters=arch.num_cluster_x, nb_y_clusters=arch.num_cluster_y)

        #Debug Memory
        debug_mem = memory.memory.Memory(self,'debug_mem', size=1)

        ############
        # Bindings #
        ############

        #Instruction Memory
        instr_router.o_MAP(instr_mem.i_INPUT(), base=arch.instruction_mem_base, size=arch.instruction_mem_size, rm_base=True)

        #Debug memory
        instr_router.o_MAP(debug_mem.i_INPUT())

        #Control register
        instr_router.o_MAP(csr.i_INPUT(), base=arch.soc_register_base, size=arch.soc_register_size, rm_base=True)
        for cluster_id in range(num_clusters):
            csr.o_BARRIER_ACK(cluster_list[cluster_id].i_SYNC_IRQ())
            pass

        #Binary loader
        loader.o_OUT(instr_router.i_INPUT())
        for cluster_id in range(num_clusters):
            loader.o_START(cluster_list[cluster_id].i_FETCHEN())
            pass

        #Clusters
        for cluster_id in range(num_clusters):
            cluster_list[cluster_id].o_NARROW_SOC(instr_router.i_INPUT())
            pass

        #Synchornization bus
        for node_id in range(num_clusters):
            cluster_list[node_id].o_SYNC_OUTPUT(sync_bus.i_INPUT())
            sync_bus.o_MAP(sync_mem_list[node_id].i_INPUT(), base=arch.sync_base+node_id*arch.sync_interleave, size=arch.sync_interleave, rm_base=True)
            pass
        

        #NoC
        for node_id in range(num_clusters):
            x_id = int(node_id%arch.num_cluster_x)
            y_id = int(node_id/arch.num_cluster_x)
            cluster_list[node_id].o_WIDE_SOC(noc.i_CLUSTER_INPUT(x_id, y_id))
            noc.o_MAP(hbm_list[node_id].i_INPUT(), base=arch.hbm_start_base+node_id*arch.cluster_tcdm_size, size=arch.cluster_tcdm_size,x=x_id+1, y=y_id+1)
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