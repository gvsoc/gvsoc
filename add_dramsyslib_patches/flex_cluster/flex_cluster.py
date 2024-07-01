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

GAPY_TARGET = True


class FlexClusterSystem(gvsoc.systree.Component):

    def __init__(self, parent, name, parser):
        super().__init__(parent, name)

        #################
        # Configuration #
        #################

        num_cluster_x           = 2
        num_cluster_y           = 2
        noc_link_width          = 512
        num_core_per_cluster    = 8

        instruction_mem_base    = 0x8000_0000
        instruction_mem_size    = 0x0001_0000

        cluster_tcdm_base       = 0x0000_0000
        cluster_tcdm_size       = 0x0002_0000

        cluster_stack_base      = 0x1000_0000

        soc_register_base       = 0x9000_0000
        soc_register_size       = 0x0001_0000

        # meta parameters
        num_clusters    = num_cluster_x * num_cluster_y

        # Get Binary
        binary = None
        if parser is not None:
            [args, otherArgs] = parser.parse_known_args()
            binary = args.binary

        ##############
        # Components #
        ##############

        #loader
        loader = utils.loader.loader.ElfLoader(self, 'loader', binary=binary)

        #instruction memory
        instr_mem = memory.memory.Memory(self, 'instr_mem', size=instruction_mem_size, atomics=True)

        #clusters
        cluster_list=[]
        for cluster_id in range(num_clusters):
            arch = ClusterArch( nb_core_per_cluster =   num_core_per_cluster,
                                base                =   cluster_tcdm_base + cluster_id * cluster_tcdm_size * 2,
                                first_hartid        =   cluster_id * num_core_per_cluster,
                                tcdm_size           =   cluster_tcdm_size,
                                stack_base          =   cluster_stack_base)
            cluster_list.append(ClusterUnit(self,f'cluster_{cluster_id}', arch))
            pass

        #instruction router
        instr_router = router.Router(self, 'instr_router')

        #control register
        csr = CtrlRegisters(self, 'ctrl_registers')

        ############
        # Bindings #
        ############

        instr_router.o_MAP(instr_mem.i_INPUT(), base=instruction_mem_base, size=instruction_mem_size, rm_base=True)
        instr_router.o_MAP(csr.i_INPUT(), base=soc_register_base, size=soc_register_size, rm_base=True)

        # Binary loader
        loader.o_OUT(instr_router.i_INPUT())
        for cluster_id in range(num_clusters):
            loader.o_START(cluster_list[cluster_id].i_FETCHEN())
            pass

        #Clusters
        for cluster_id in range(num_clusters):
            cluster_list[cluster_id].o_NARROW_SOC(instr_router.i_INPUT())
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