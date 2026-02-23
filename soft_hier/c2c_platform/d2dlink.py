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

class D2DLink(gvsoc.systree.Component):
    def __init__(self, parent, name,
            link_id: int=0,
            link_type: str='default',
            fifo_depth_rx: int=64,
            fifo_depth_tx: int=64,
            fifo_credit_bar: int=10,
            flit_granularity_byte: int=64,
            link_latency_ns: int=256,
            link_bandwidth_GBps: int=256):
        #Initialize the parent class
        super(D2DLink, self).__init__(parent, name)

        # Define properties
        self.add_property('link_id',                link_id)
        self.add_property('link_type',              link_type)
        self.add_property('fifo_depth_rx',          fifo_depth_rx)
        self.add_property('fifo_depth_tx',          fifo_depth_tx)
        self.add_property('fifo_credit_bar',        fifo_credit_bar)
        self.add_property('flit_granularity_byte',  flit_granularity_byte)
        self.add_property('link_latency_ns',        link_latency_ns)
        self.add_property('link_bandwidth_GBps',    link_bandwidth_GBps)

        # Add sources
        self.add_sources(['pulp/c2c_platform/d2dlink.cpp'])

    def i_DATA_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'data_in', signature='io')

    def o_DATA_OUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('data_out', itf, signature='io')
