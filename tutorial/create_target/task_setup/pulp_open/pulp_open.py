#
# Copyright (C) 2020 GreenWaves Technologies
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

import gvsoc.systree as st

#### TASK - Change the path from pulp.chips.pulp_open.soc to pulp.chips.pulp_open_hwpe.soc

from pulp.chips.pulp_open.soc import Soc

#### TASK - Change the path from pulp.chips.pulp_open.cluster to pulp.chips.pulp_open_hwpe.cluster

from pulp.chips.pulp_open.cluster import Cluster, get_cluster_name
from vp.clock_domain import Clock_domain
from utils.clock_generator import Clock_generator
from pulp.padframe.padframe_v1 import Padframe
import interco.router_proxy as router_proxy
import memory.dramsys

class Pulp_open(st.Component):

#### TASK - Change the path from soc_config_file='pulp/chips/pulp_open/soc.json to soc_config_file='pulp/chips/pulp_open_hwpe/soc.json'
#### TASK - Change the path from cluster_config_file='pulp/chips/pulp_open/cluster.json' to cluster_config_file='pulp/chips/pulp_open_hwpe/cluster.json'
#### TASK - Change the path from padframe_config_file='pulp/chips/pulp_open/padframe.json' to padframe_config_file='pulp/chips/pulp_open_hwpe/padframe.json'

    def __init__(self, parent, name, parser, soc_config_file='pulp/chips/pulp_open/soc.json',
            cluster_config_file='pulp/chips/pulp_open/cluster.json', padframe_config_file='pulp/chips/pulp_open/padframe.json',
            use_ddr=False):
        super(Pulp_open, self).__init__(parent, name)

        #
        # Properties
        #

        soc_config_file = self.add_property('soc_config_file', soc_config_file)
        cluster_config_file = self.add_property('cluster_config_file', cluster_config_file)
        nb_cluster = self.add_property('nb_cluster', 1)

    
        #
        # Components
        #

        # Padframe
        padframe = Padframe(self, 'padframe', config_file=padframe_config_file)

        # Soc clock domain
        soc_clock = Clock_domain(self, 'soc_clock_domain', frequency=50000000)

        # Clusters clock domains
        cluster_clocks = []
        for cid in range(0, nb_cluster):
            cluster_name = get_cluster_name(cid)
            cluster_clocks.append(Clock_domain(self, cluster_name + '_clock', frequency=50000000))

        # Clusters
        clusters = []
        for cid in range(0, nb_cluster):
            cluster_name = get_cluster_name(cid)
            clusters.append(Cluster(self, cluster_name, config_file=cluster_config_file, cid=cid))

        # Soc
        soc = Soc(self, 'soc', parser, config_file=soc_config_file, chip=self, cluster=clusters[0])

        # Fast clock
        fast_clock = Clock_domain(self, 'fast_clock', frequency=24576063*2)
        fast_clock_generator = Clock_generator(self, 'fast_clock_generator', powered_on=False, powerup_time=200000000)

        # Ref clock
        ref_clock = Clock_domain(self, 'ref_clock', frequency=65536)
        ref_clock_generator = Clock_generator(self, 'ref_clock_generator')

        # AXI proxy
        axi_proxy = router_proxy.Router_proxy(self, 'axi_proxy')

        # DRAMsys
        if use_ddr:
            ddr = memory.dramsys.Dramsys(self, 'ddr')


        #
        # Bindings
        #

        # Padframe
        self.bind(ref_clock_generator, 'clock_sync', padframe, 'ref_clock_pad')
        self.bind(padframe, 'ref_clock', soc, 'ref_clock')

        for name, group in padframe.get_property('groups').items():
            pad_type = group.get('type')
            nb_cs = group.get('nb_cs')
            is_master = group.get('is_master')
            is_slave = group.get('is_slave')
            is_dual = group.get('is_dual')

            if pad_type == 'gpio':
                self.bind(padframe, name + '_pad', soc, name)
            else:
                if is_master:
                    self.bind(soc, name, padframe, name)
                    if is_dual:
                        self.bind(padframe, name + '_in', soc, name + '_in')
                if is_slave:
                    self.bind(padframe, name, soc, name)
                    if is_dual:
                        self.bind(soc, name + '_out', padframe, name + '_out')

            if nb_cs is not None:
                for cs in range(0, nb_cs):
                    cs_name = name + '_cs' + str(cs)
                    cs_data_name = name + '_cs' + str(cs) + '_data'
                    if is_master:
                        self.bind(padframe, cs_data_name + '_pad', self, cs_data_name)
                        self.bind(padframe, cs_name + '_pad', self, cs_name)
                    if is_slave:
                        self.bind(self, cs_data_name, padframe, cs_data_name + '_pad')
                        self.bind(self, cs_name, padframe, cs_name + '_pad')
            else:
                if is_master:
                    self.bind(padframe, name + '_pad', self, name)
                if is_slave:
                    self.bind(self, name, padframe, name + '_pad')

        # Soc clock domain
        self.bind(soc_clock, 'out', soc, 'clock')
        self.bind(soc_clock, 'out', axi_proxy, 'clock')
        if use_ddr:
            self.bind(soc_clock, 'out', ddr, 'clock')

        # Clusters
        for cid in range(0, nb_cluster):
            cluster = clusters[cid]
            self.bind(ref_clock_generator, 'clock_sync', cluster, 'ref_clock')
            self.bind(cluster, 'dma_irq', soc, 'dma_irq')
            for pe in range(0, clusters[0].get_property('nb_pe', int)):
                self.bind(soc, 'halt_cluster%d_pe%d' % (cid, pe), cluster, 'halt_pe%d' % pe)

            self.bind(cluster_clocks[cid], 'out', clusters[cid], 'clock')
            self.bind(soc, get_cluster_name(cid) + '_fll', cluster_clocks[cid], 'clock_in')
            self.bind(soc, get_cluster_name(cid) + '_input', clusters[cid], 'input')
            self.bind(clusters[cid], 'soc', soc, 'soc_input')

        # Soc
        self.bind(soc, 'fast_clk_ctrl', fast_clock_generator, 'power')
        self.bind(soc, 'ref_clk_ctrl', ref_clock_generator, 'power')
        self.bind(soc, 'fll_soc_clock', soc_clock, 'clock_in')

        # Fast clock
        self.bind(fast_clock, 'out', fast_clock_generator, 'clock')
        self.bind(fast_clock_generator, 'clock_sync', soc, 'fast_clock')
        self.bind(fast_clock_generator, 'clock_ctrl', fast_clock, 'clock_in')

        # Ref clock
        self.bind(ref_clock, 'out', ref_clock_generator, 'clock')

        # SOC
        self.bind(self, 'bootsel', soc, 'bootsel')
        self.bind(fast_clock, 'out', soc, 'fast_clock_out')
        self.bind(axi_proxy, 'out', soc, 'soc_input')
        self.bind(soc, 'axi_proxy', axi_proxy, 'input')
        if use_ddr:
            self.bind(soc, 'ddr', ddr, 'input')


    def gen_gtkw_conf(self, tree, traces):
        if tree.get_view() == 'overview':
            self.vcd_group(self, skip=True)
        else:
            self.vcd_group(self, skip=False)
