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
from pulp.c2c_platform.endpoint import Endpoint


class FatTree(gvsoc.systree.Component):
    def __init__(self, parent, name, radix, level, pos_idx = 0, pos_lvl = 0, cfg = None):
        super(FatTree, self).__init__(parent, name)
        self.cfg            = cfg
        self.radix          = radix
        self.level          = level
        self.half_radix     = radix // 2
        self.endpoints      = 2 * (self.half_radix ** level)
        self.num_top_router = self.half_radix ** (level - 1)
        self.num_mid_router = 2 * (self.half_radix ** (level - 1))
        self.num_edg_router = 2 * (self.half_radix ** (level - 1))
        self.add_property('radix', radix)
        self.add_property('level', level)
        self.add_property('half_radix', self.half_radix)
        self.add_property('endpoints', self.endpoints)
        self.add_property('num_top_router', self.num_top_router)
        self.add_property('num_mid_router', self.num_mid_router)
        self.add_property('pos_idx', pos_idx)
        self.add_property('pos_lvl', pos_lvl)
        self.add_sources(['pulp/c2c_platform/topology_manager/fattree.cpp'])

    def i_TOP_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'in', signature='io')
    
    def reshape(self, lst, n_cols):
        return [lst[i:i + n_cols] for i in range(0, len(lst), n_cols)]
    
    def instantiate_routers(self, parent, num_routers, level):
        router_list = []
        topology_list = []
        for i in range(num_routers):
            #a. instantiate router
            router = Router(parent, f"router_lvl{level}_idx{i}", num_port=self.radix, rx_depth=2)

            #b. instantiate topology
            topology = FatTree(parent, f"top_lvl{level}_idx{i}", radix=self.radix, level=self.level, pos_idx=i, pos_lvl=level)

            #c. connect router and topology
            router.o_TOP_OUT(topology.i_TOP_INPUT())

            #d. append list
            router_list.append(router)
            topology_list.append(topology)
            pass
        return router_list
    
    def connect_endpoints_mid_routers(self, parent, endpoint_list, router_list):
        grouped_endpoints = self.reshape(endpoint_list, self.half_radix)
        for rid in range(len(router_list)):
            for eid in range(self.half_radix):
                endpoint = grouped_endpoints[rid][eid]
                router   = router_list[rid]
                # link endpoint to router
                d2dlink_e2r = D2DLink(parent, f"link_{endpoint.name}_to_{router.name}",
                    link_id=eid + rid * self.half_radix,
                    fifo_depth_rx=self.cfg.link_depth_rx,
                    fifo_depth_tx=self.cfg.link_depth_tx,
                    fifo_credit_bar=self.cfg.link_credit_bar,
                    flit_granularity_byte=self.cfg.flit_granularity_byte,
                    link_latency_ns=self.cfg.local_latency_ns,
                    link_bandwidth_GBps=self.cfg.link_bandwidth_GBps)
                endpoint.o_DATA_OUT(d2dlink_e2r.i_DATA_INPUT())
                d2dlink_e2r.o_DATA_OUT(router.i_PORT_INPUT(eid))
                # link router to endpoint
                d2dlink_r2e = D2DLink(parent, f"link_{router.name}_to_{endpoint.name}",
                    link_id=eid + rid * self.half_radix,
                    fifo_depth_rx=self.cfg.link_depth_rx,
                    fifo_depth_tx=self.cfg.link_depth_tx,
                    fifo_credit_bar=self.cfg.link_credit_bar,
                    flit_granularity_byte=self.cfg.flit_granularity_byte,
                    link_latency_ns=self.cfg.local_latency_ns,
                    link_bandwidth_GBps=self.cfg.link_bandwidth_GBps)
                router.o_PORT_OUT(d2dlink_r2e.i_DATA_INPUT(),eid)
                d2dlink_r2e.o_DATA_OUT(endpoint.i_DATA_INPUT())
                pass
            pass
        pass
    
    def connect_endpoints_top_routers(self, parent, endpoint_list, top_router_list):
        grouped_endpoints = self.reshape(endpoint_list, self.radix)
        for rid in range(len(top_router_list)):
            for eid in range(self.radix):
                endpoint = grouped_endpoints[rid][eid]
                router   = top_router_list[rid]
                # link endpoint to router
                d2dlink_e2r = D2DLink(parent, f"link_{endpoint.name}_to_{router.name}",
                    link_id=eid + rid * self.radix,
                    fifo_depth_rx=self.cfg.link_depth_rx,
                    fifo_depth_tx=self.cfg.link_depth_tx,
                    fifo_credit_bar=self.cfg.link_credit_bar,
                    flit_granularity_byte=self.cfg.flit_granularity_byte,
                    link_latency_ns=self.cfg.local_latency_ns,
                    link_bandwidth_GBps=self.cfg.link_bandwidth_GBps)
                endpoint.o_DATA_OUT(d2dlink_e2r.i_DATA_INPUT())
                d2dlink_e2r.o_DATA_OUT(router.i_PORT_INPUT(eid))
                # link router to endpoint
                d2dlink_r2e = D2DLink(parent, f"link_{router.name}_to_{endpoint.name}",
                    link_id=eid + rid * self.radix,
                    fifo_depth_rx=self.cfg.link_depth_rx,
                    fifo_depth_tx=self.cfg.link_depth_tx,
                    fifo_credit_bar=self.cfg.link_credit_bar,
                    flit_granularity_byte=self.cfg.flit_granularity_byte,
                    link_latency_ns=self.cfg.local_latency_ns,
                    link_bandwidth_GBps=self.cfg.link_bandwidth_GBps)
                router.o_PORT_OUT(d2dlink_r2e.i_DATA_INPUT(),eid)
                d2dlink_r2e.o_DATA_OUT(endpoint.i_DATA_INPUT())
                pass
            pass
        pass
    
    def connect_mid_mid_routers(self, parent, down_list, up_list, down_level):
        group_len         = self.half_radix ** (down_level + 1)
        grouped_up_list   = self.reshape(up_list, group_len)
        grouped_down_list = self.reshape(down_list, group_len)
        for gid in range(len(grouped_up_list)):
            sub_up_list = grouped_up_list[gid]
            sub_down_list = grouped_down_list[gid]
            for drid in range(group_len):
                for dlid in range(self.half_radix):
                    down_router = sub_down_list[drid]
                    down_link_port = dlid + self.half_radix
                    urid = (drid * self.half_radix + dlid) % group_len
                    ulid = (drid * self.half_radix + dlid) // group_len
                    up_router = sub_up_list[urid]
                    up_link_port = ulid
                    d2dlink_d2u = D2DLink(parent, f"link_{down_router.name}_to_{up_router.name}",
                        fifo_depth_rx=self.cfg.link_depth_rx,
                        fifo_depth_tx=self.cfg.link_depth_tx,
                        fifo_credit_bar=self.cfg.link_credit_bar,
                        flit_granularity_byte=self.cfg.flit_granularity_byte,
                        link_latency_ns=self.cfg.link_latency_ns,
                        link_bandwidth_GBps=self.cfg.link_bandwidth_GBps)
                    down_router.o_PORT_OUT(d2dlink_d2u.i_DATA_INPUT(),down_link_port)
                    d2dlink_d2u.o_DATA_OUT(up_router.i_PORT_INPUT(up_link_port))
                    d2dlink_u2d = D2DLink(parent, f"link_{up_router.name}_to_{down_router.name}",
                        fifo_depth_rx=self.cfg.link_depth_rx,
                        fifo_depth_tx=self.cfg.link_depth_tx,
                        fifo_credit_bar=self.cfg.link_credit_bar,
                        flit_granularity_byte=self.cfg.flit_granularity_byte,
                        link_latency_ns=self.cfg.link_latency_ns,
                        link_bandwidth_GBps=self.cfg.link_bandwidth_GBps)
                    up_router.o_PORT_OUT(d2dlink_u2d.i_DATA_INPUT(),up_link_port)
                    d2dlink_u2d.o_DATA_OUT(down_router.i_PORT_INPUT(down_link_port))
                    pass
                pass
            pass
        pass
    
    def connect_mid_top_routers(self, parent, router_list, top_list):
        for drid in range(len(router_list)):
            for dlid in range(self.half_radix):
                down_router = router_list[drid]
                down_link_port = dlid + self.half_radix
                urid = (drid * self.half_radix + dlid) % len(top_list)
                ulid = (drid * self.half_radix + dlid) // len(top_list)
                up_router = top_list[urid]
                up_link_port = ulid
                d2dlink_d2u = D2DLink(parent, f"link_{down_router.name}_to_{up_router.name}",
                    fifo_depth_rx=self.cfg.link_depth_rx,
                    fifo_depth_tx=self.cfg.link_depth_tx,
                    fifo_credit_bar=self.cfg.link_credit_bar,
                    flit_granularity_byte=self.cfg.flit_granularity_byte,
                    link_latency_ns=self.cfg.link_latency_ns,
                    link_bandwidth_GBps=self.cfg.link_bandwidth_GBps)
                down_router.o_PORT_OUT(d2dlink_d2u.i_DATA_INPUT(),down_link_port)
                d2dlink_d2u.o_DATA_OUT(up_router.i_PORT_INPUT(up_link_port))
                d2dlink_u2d = D2DLink(parent, f"link_{up_router.name}_to_{down_router.name}",
                    fifo_depth_rx=self.cfg.link_depth_rx,
                    fifo_depth_tx=self.cfg.link_depth_tx,
                    fifo_credit_bar=self.cfg.link_credit_bar,
                    flit_granularity_byte=self.cfg.flit_granularity_byte,
                    link_latency_ns=self.cfg.link_latency_ns,
                    link_bandwidth_GBps=self.cfg.link_bandwidth_GBps)
                up_router.o_PORT_OUT(d2dlink_u2d.i_DATA_INPUT(),up_link_port)
                d2dlink_u2d.o_DATA_OUT(down_router.i_PORT_INPUT(down_link_port))
                pass
            pass
        pass

    def build_top(self, parent, endpoint_list):
        if(len(endpoint_list) < self.endpoints):
            #Insert Dummy Endpoints
            start_id = len(endpoint_list)
            for i in range(self.endpoints - start_id):
                eid = start_id + i
                endpoint = Endpoint(parent, f"endpoint_{eid}_dummy", endpoint_id=eid, endpoint_type = 'dummy', num_tx_flit=0, flit_granularity_byte=self.cfg.flit_granularity_byte)
                endpoint_list.append(endpoint)
                pass
            pass
        assert(len(endpoint_list) == self.endpoints)

        if(self.level == 1):
            #Build top router, topoogy and connect to endpoints
            top_router_list = self.instantiate_routers(parent, self.num_top_router, self.level - 1)
            self.connect_endpoints_top_routers(parent, endpoint_list, top_router_list)
            pass
        elif(self.level > 1):
            #1. Build edge router, topolpgy and connect to endpoints
            latest_routers = self.instantiate_routers(parent, self.num_edg_router, 0)
            self.connect_endpoints_mid_routers(parent, endpoint_list, latest_routers)

            #2. Iterate over mid levels
            for i in range(self.level - 2):
                mid_level = i + 1
                mid_router_list = self.instantiate_routers(parent, self.num_mid_router, mid_level)
                self.connect_mid_mid_routers(parent, latest_routers, mid_router_list, i)
                latest_routers = mid_router_list
                pass

            #3. Build top router, topoogy and connect to latest level
            top_router_list = self.instantiate_routers(parent, self.num_top_router, self.level - 1)
            self.connect_mid_top_routers(parent, latest_routers, top_router_list)
            pass
        else:
            raise RuntimeError(f"Invalid FatTree Level {self.level}")
        pass
