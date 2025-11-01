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
import memory.dramsys
from pulp.c2c_platform.platformctrl import PlatformCtrl
from pulp.c2c_platform.d2dlink import D2DLink
from pulp.c2c_platform.endpoint import Endpoint
from pulp.c2c_platform.c2c_platform_cfg import C2CPlatformCFG
import math

GAPY_TARGET = True

class C2CSystem(gvsoc.systree.Component):

    def __init__(self, parent, name, parser):
        super().__init__(parent, name)

        #################
        # Configuration #
        #################

        cfg = C2CPlatformCFG()

        ##############
        # Components #
        ##############

        # Platform Controller
        platform_ctrl = PlatformCtrl(self, "ctrl", cfg.num_chip)

        # Endpoints
        endpoint_list = []
        for eid in range(cfg.num_chip):
            endpoint = Endpoint(self, f"endpoint_{eid}",
                endpoint_id=eid,
                num_tx_flit=2000,
                flit_granularity_byte=cfg.flit_granularity_byte)
            endpoint_list.append(endpoint)
            pass

        #######################
        # Controller Bindings #
        #######################

        for eid in range(cfg.num_chip):
            platform_ctrl.o_START(endpoint_list[eid].i_START(), eid)
            endpoint_list[eid].o_BARRIER_REQ(platform_ctrl.i_BARRIER_ACK(eid))
            pass

        ############
        # Topology #
        ############

        if cfg.topology == 'self-link':
            # Self-Linking
            for eid in range(cfg.num_chip):
                d2dlink = D2DLink(self, f"d2dlink_{eid}_to_{eid}", 
                    link_id=eid,
                    fifo_depth_rx=cfg.link_depth_rx,
                    fifo_depth_tx=cfg.link_depth_tx,
                    fifo_credit_bar=cfg.link_credit_bar,
                    flit_granularity_byte=cfg.flit_granularity_byte,
                    link_latency_ns=cfg.link_latency_ns,
                    link_bandwidth_GBps=cfg.link_bandwidth_GBps)
                endpoint_list[eid].o_DATA_OUT(d2dlink.i_DATA_INPUT())
                d2dlink.o_DATA_OUT(endpoint_list[eid].i_DATA_INPUT())
                pass
        else:
            raise RuntimeError(f"C2C Topology {cfg.topology} currently not supported")
            pass



class C2CPlatform(gvsoc.systree.Component):

    def __init__(self, parent, name, parser, options):
        super(C2CPlatform, self).__init__(parent, name, options=options)

        clock = Clock_domain(self, 'clock', frequency=1000000000)

        c2c_system = C2CSystem(self, 'chip', parser)

        self.bind(clock, 'out', c2c_system, 'clock')

class Target(gvsoc.runner.Target):

    def __init__(self, parser, options):
        super(Target, self).__init__(parser, options,
            model=C2CPlatform, description="Chiplet Chip-to-Chip Interconect Platform")
