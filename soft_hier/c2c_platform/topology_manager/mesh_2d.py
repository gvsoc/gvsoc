#
# Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
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

import gvsoc.systree
from pulp.c2c_platform.d2dlink import D2DLink
from pulp.c2c_platform.router import Router

port_map = {
    "local"     : 0,
    "west"      : 1,
    "north"     : 2,
    "east"      : 3,
    "south"     : 4,
}

class Mesh2D(gvsoc.systree.Component):
    def __init__(self, parent, name, x_dim, y_dim, x_pos, y_pos):
        super(Mesh2D, self).__init__(parent, name)
        self.add_property('x_dim', x_dim)
        self.add_property('y_dim', y_dim)
        self.add_property('x_pos', x_pos)
        self.add_property('y_pos', y_pos)
        self.add_sources('pulp/c2c_platform/topology_manager/mesh_2d.cpp')

    def i_TOP_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'in', signature='io')

    def id2pos(self, tid, cfg):
        x = tid % cfg.num_chip_x
        y = tid // cfg.num_chip_x
        return (x, y)
        pass

    def pos2id(self, pos, cfg):
        return pos[1] * cfg.num_chip_x + pos[0]
        pass

    def build_top(self, parent, endpoint_list, cfg):
        assert(len(endpoint_list) == cfg.num_chip_x * cfg.num_chip_y)

        # instantiate topology managers
        top_list = []
        for x in range(cfg.num_chip_x):
            top_list.append([])
            for y in range(cfg.num_chip_y):
                top = Mesh2D(parent, f"top_{x}_{y}"
                    x_dim = cfg.num_chip_x,
                    y_dim = cfg.num_chip_y,
                    x_pos = x,
                    y_pos = y)
                top_list[x].append(top)
                pass
            pass

        # instanciate routers
        router_list = []
        for x in range(cfg.num_chip_x):
            router_list.append([])
            for y in range(cfg.num_chip_y):
                router = Router(parent, f"router_{x}_{y}", num_port=5, rx_depth=2)
                router_list[x].append(router)
                pass
            pass

        # bind router <--> topology manager
        for x in range(cfg.num_chip_x):
            for y in range(cfg.num_chip_y):
                router_list[x][y].o_TOP_OUT(top_list[x][y].i_TOP_INPUT())
                pass
            pass

        # bind router <--> endpoints
        for x in range(cfg.num_chip_x):
            for y in range(cfg.num_chip_y):
                eid = self.pos2id((x,y), cfg)
                d2dlink_e2r = D2DLink(self, f"d2dlink_{x}_{y}_e2r",
                    link_id=eid,
                    fifo_depth_rx=cfg.link_depth_rx,
                    fifo_depth_tx=cfg.link_depth_tx,
                    fifo_credit_bar=cfg.link_credit_bar,
                    flit_granularity_byte=cfg.flit_granularity_byte,
                    link_latency_ns=cfg.local_latency_ns,
                    link_bandwidth_GBps=cfg.link_bandwidth_GBps)
                d2dlink_r2e = D2DLink(self, f"d2dlink_{x}_{y}_r2e",
                    link_id=eid,
                    fifo_depth_rx=cfg.link_depth_rx,
                    fifo_depth_tx=cfg.link_depth_tx,
                    fifo_credit_bar=cfg.link_credit_bar,
                    flit_granularity_byte=cfg.flit_granularity_byte,
                    link_latency_ns=cfg.local_latency_ns,
                    link_bandwidth_GBps=cfg.link_bandwidth_GBps)
                endpoint_list[eid].o_DATA_OUT(d2dlink_e2r.i_DATA_INPUT())
                d2dlink_e2r.o_DATA_OUT(router_list[x][y].i_PORT_INPUT(port_map["local"]))
                router_list[x][y].o_PORT_OUT(d2dlink_r2e.i_DATA_INPUT(),port_map["local"])
                d2dlink_r2e.o_DATA_OUT(endpoint_list[eid].i_DATA_INPUT())
                pass
            pass

        # bind router <--> router
        for x in range(cfg.num_chip_x):
            for y in range(cfg.num_chip_y):
                eid = self.pos2id((x,y), cfg)
                # to west
                if x > 0:
                    d2dlink = D2DLink(self, f"d2dlink_{x}_{y}_west",
                        link_id=eid,
                        fifo_depth_rx=cfg.link_depth_rx,
                        fifo_depth_tx=cfg.link_depth_tx,
                        fifo_credit_bar=cfg.link_credit_bar,
                        flit_granularity_byte=cfg.flit_granularity_byte,
                        link_latency_ns=cfg.link_latency_ns,
                        link_bandwidth_GBps=cfg.link_bandwidth_GBps)
                    router_list[x][y].o_PORT_OUT(d2dlink.i_DATA_INPUT(),port_map["west"])
                    d2dlink.o_DATA_OUT(router_list[x-1][y].i_PORT_INPUT(port_map["east"]))
                    pass
                # to south
                if y > 0:
                    d2dlink = D2DLink(self, f"d2dlink_{x}_{y}_south",
                        link_id=eid,
                        fifo_depth_rx=cfg.link_depth_rx,
                        fifo_depth_tx=cfg.link_depth_tx,
                        fifo_credit_bar=cfg.link_credit_bar,
                        flit_granularity_byte=cfg.flit_granularity_byte,
                        link_latency_ns=cfg.link_latency_ns,
                        link_bandwidth_GBps=cfg.link_bandwidth_GBps)
                    router_list[x][y].o_PORT_OUT(d2dlink.i_DATA_INPUT(),port_map["south"])
                    d2dlink.o_DATA_OUT(router_list[x][y-1].i_PORT_INPUT(port_map["north"]))
                    pass
                # to east
                if x < (cfg.num_chip_x - 1):
                    d2dlink = D2DLink(self, f"d2dlink_{x}_{y}_east",
                        link_id=eid,
                        fifo_depth_rx=cfg.link_depth_rx,
                        fifo_depth_tx=cfg.link_depth_tx,
                        fifo_credit_bar=cfg.link_credit_bar,
                        flit_granularity_byte=cfg.flit_granularity_byte,
                        link_latency_ns=cfg.link_latency_ns,
                        link_bandwidth_GBps=cfg.link_bandwidth_GBps)
                    router_list[x][y].o_PORT_OUT(d2dlink.i_DATA_INPUT(),port_map["east"])
                    d2dlink.o_DATA_OUT(router_list[x+1][y].i_PORT_INPUT(port_map["west"]))
                    pass
                # to north
                if y < (cfg.num_chip_y - 1):
                    d2dlink = D2DLink(self, f"d2dlink_{x}_{y}_north",
                        link_id=eid,
                        fifo_depth_rx=cfg.link_depth_rx,
                        fifo_depth_tx=cfg.link_depth_tx,
                        fifo_credit_bar=cfg.link_credit_bar,
                        flit_granularity_byte=cfg.flit_granularity_byte,
                        link_latency_ns=cfg.link_latency_ns,
                        link_bandwidth_GBps=cfg.link_bandwidth_GBps)
                    router_list[x][y].o_PORT_OUT(d2dlink.i_DATA_INPUT(),port_map["north"])
                    d2dlink.o_DATA_OUT(router_list[x][y+1].i_PORT_INPUT(port_map["south"]))
                    pass
                pass
            pass
        pass
