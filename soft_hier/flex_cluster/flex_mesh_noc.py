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

from pulp.floonoc.floonoc import FlooNoc2dMesh
import gvsoc.systree

class FlexMeshNoC(FlooNoc2dMesh):
    """FlooNoc instance for a grid of clusters

    This instantiates a FlooNoc 2D mesh for a grid of clusters.
    It contains:
     - a central part made of one cluster, one router and one network interface at each node
     - a border of targets
    If the grid contains X by Y clusters, the whole grid is X+2 by Y+2 as there are borders in each
    direction for the targets.

    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    width: int
        The width in bytes of the interconnect. This gives the number of bytes/cycles each node can
        route.
    nb_x_clusters: int
        Number of clusters on the X direction. This should not include the targets on the borders.
    nb_y_clusters: int
        Number of clusters on the Y direction. This should not include the targets on the borders.
    ni_outstanding_reqs: int
        Number of outstanding requests each network interface can inject to the routers. This should
        be increased when the size of the noc increases.
    router_input_queue_size: int
        Size of the routers input queues. This gives the number of requests which can be buffered
        before the source output queue is stalled.
    """
    def __init__(self, parent: gvsoc.systree.Component, name, width: int, nb_x_clusters: int,
            nb_y_clusters: int, ni_outstanding_reqs: int=2, router_input_queue_size: int=2, atomics: int=0, collective: int=0,
            interleave_enable: int=0, interleave_region_base: int=0, interleave_region_size: int=0, interleave_granularity: int=0, interleave_bit_start: int=0, interleave_bit_width: int=0):
        # The total grid contains 1 more node on each direction for the targets
        super(FlexMeshNoC, self).__init__(parent, name, width, dim_x=nb_x_clusters+2, dim_y=nb_y_clusters+2,
                                          ni_outstanding_reqs=ni_outstanding_reqs, router_input_queue_size=router_input_queue_size, atomics=atomics, collective=collective,
                                          interleave_enable=interleave_enable, interleave_region_base=interleave_region_base, interleave_region_size=interleave_region_size, interleave_granularity=interleave_granularity, interleave_bit_start=interleave_bit_start, interleave_bit_width=interleave_bit_width)

        for tile_x in range(0, nb_x_clusters):
            for tile_y in range(0, nb_y_clusters):
                # Add 1 as clusters, routers and network_interfaces are in the central part
                self.add_router(tile_x+1, tile_y+1)
                self.add_network_interface(tile_x+1, tile_y+1)

    def i_CLUSTER_INPUT(self, x: int, y: int) -> gvsoc.systree.SlaveItf:
        """Returns the input port of a cluster tile.

        The cluster can inject requests to the noc using this interface. The noc will then
        forward it to the right target.

        Returns
        ----------
        gvsoc.systree.SlaveItf
            The slave interface
        """
        return self.i_INPUT(x+1, y+1)