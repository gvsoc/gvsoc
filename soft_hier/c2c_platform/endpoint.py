import os

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

import os
import subprocess
import gvsoc.systree

class Endpoint(gvsoc.systree.Component):
    def __init__(self, parent, name,
            endpoint_id: int=0,
            endpoint_type: str='default',
            trace_file: str='',
            num_tx_flit: int=1000,
            flit_granularity_byte: int=64):
        super(Endpoint, self).__init__(parent, name)

        self.add_property('endpoint_id',            endpoint_id)
        self.add_property('endpoint_type',          endpoint_type)
        if len(trace_file) > 0:
            if os.path.exists(trace_file):
                # If it exists, use the absolute path
                self.add_property('use_trace_file',     1)
                self.add_property('trace_file',         os.path.abspath(trace_file))
                # Get the number of lines in the trace file
                num_lines = int(subprocess.check_output(['wc', '-l', trace_file]).split()[0])
                num_tx_flit = num_lines
            else:
                raise RuntimeError(f"Trace file {trace_file} does not exist")
            pass
        else:
            self.add_property('use_trace_file',     0)
            self.add_property('trace_file',         trace_file)
        self.add_property('num_tx_flit',            num_tx_flit)
        self.add_property('flit_granularity_byte',  flit_granularity_byte)

        self.add_sources(['pulp/c2c_platform/endpoint.cpp'])

    def i_DATA_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'data_in', signature='io')

    def o_DATA_OUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('data_out', itf, signature='io')

    def o_BARRIER_REQ(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind(f'barrier_req', itf, signature='wire<bool>')

    def i_START(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, f'start', signature='wire<bool>')
