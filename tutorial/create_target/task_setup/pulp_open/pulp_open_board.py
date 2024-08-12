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

import gvsoc.systree as st

#### TASK - Change the path from pulp.chips.pulp_open.pulp_open to pulp.chips.pulp_open_hwpe.pulp_open

from pulp.chips.pulp_open.pulp_open import Pulp_open
from devices.hyperbus.hyperflash import Hyperflash
from devices.spiflash.spiflash import Spiflash
from devices.spiflash.atxp032 import Atxp032
from devices.hyperbus.hyperram import Hyperram
from devices.testbench.testbench import Testbench
from devices.uart.uart_checker import Uart_checker
import gvsoc.runner
from gapylib.chips.pulp.flash import *


class Pulp_open_board(st.Component):

    def __init__(self, parent, name, parser, options, use_ddr=False):

        super(Pulp_open_board, self).__init__(parent, name, options=options)

        # Pulp
        pulp = Pulp_open(self, 'chip', parser, use_ddr=use_ddr)

        # Flash
        hyperflash = Hyperflash(self, 'hyperflash')
        hyperram = Hyperram(self, 'ram')

        self.register_flash(
            DefaultFlashRomV2(self, 'hyperflash', image_name=hyperflash.get_image_path(),
                flash_attributes={
                    "flash_type": 'hyper'
                },
                size=8*1024*1024
            ))

        self.bind(pulp, 'hyper0_cs1_data', hyperflash, 'input')

        self.bind(pulp, 'hyper0_cs0_data', hyperram, 'input')

        uart_checker = Uart_checker(self, 'uart_checker')
        self.bind(pulp, 'uart0', uart_checker, 'input')



class Pulp_open_board_ddr(Pulp_open_board):

    def __init__(self, parent, name, parser, options):
        super().__init__(parent, name, parser, options, use_ddr=True)
