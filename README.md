# GVSOC

GVSoC is the PULP chips simulator that is natively included in the Pulp SDK and is described and evaluated fully in Bruschi et al. [\[arXiv:2201.08166v1\]](https://arxiv.org/abs/2201.08166).

## [Look Here!] How to get start GVSoC with DRAMSys5 Library

### Getting Start

For a streamlined experience, you can build, run, and test the dramsys-integrated GVSoC simply by following instructions below:

**On the ETH Network:**

- Run the command:
	~~~~~shell
	source dramsys_pushbutton_ETHenv.sh
	~~~~~
    Note: If you encounter errors related to Python packages not being found, please install the necessary packages yourself.

**Outside the ETH Network:**

Ensure your system meets the following environment requirements:

- GCC version 11.2.0 or higher
- G++ version 11.2.0 or higher
- CMake version 3.18.1 or higher

Once these requirements are met, proceed by using:

~~~~~shell
source dramsys_pushbutton.sh
~~~~~

### Change DRAM Models

To help you get start with GVSoC+DRAMSys, we prepared GVSoC target `pulp-open-ddr` is a template model of Pulp-SoC connected with external DRAM models. The default configuration utilizes one HBM2 channel for the DRAM model. If you want to use different DRAM models, you can modify the code in `core/models/memory/dramsys.py` 

```python
import gvsoc.systree as st

class Dramsys(st.Component):

    def __init__(self, parent, name):

        super(Dramsys, self).__init__(parent, name)

        self.set_component('memory.dramsys')

        self.add_properties({
            'require_systemc': True,
            'dram-type': 'hbm2',
        })
```

You can change the `dram-type` to `ddr3`, `ddr4`, `hbm2`, `lpddr4`, which they models DRAM of:

- `ddr3` : DDR3 DIMM (8 x MICRON_DDR3 Chips, 1GB capacity, 1600MHz-DDR)
- `ddr4` : DDR4 DIMM (8 x JEDEC_SPEC_DDR4 Chips, 4GB capacity, 1866MHz-DDR )
- `hbm2` : Single Channel of HBM2 Stack (1GB capacity, 2000MHz-DDR)
- `lpddr4` : Single Channle of LPDDR4 Chip (1GB capacity, 3200MHz-DDR)


### Develop your GVSoC+DRAM simulation target

Once you have successfully go through `source dramsys_pushbutton.sh`, the environment for GVSoC+DRAMSys co-simulation is setup

Then You can freely develop your own model, instance multiple DRAM models and connect them. You can follow the example in `pulp/pulp/chips/pulp_open/pulp_open.py`

```python
80		ddr = memory.dramsys.Dramsys(self, 'ddr') 
130		self.bind(soc_clock, 'out', ddr, 'clock')
164		self.bind(soc, 'ddr', ddr, 'input')
```

**Note:** If you opened a new terminal/shell to your workplace, please do `source sourceme.sh` before building your GVSoC target, this will make sure neccesary environment parameters set properly for GVSoC+DRAMSys co-simulation.


### Migrate DRAMSys-Integration to other GVSoC branches

There are a lot of on-developing GVSoC branchs for different extensions. To enbale GVSoC+DRAMsys co-simulations on those branches, there are following steps to follow:

- copy folder `add_dramsyslib_patches`
- copy `dramsys_pushbutton_ETHenv.sh` and `dramsys_pushbutton.sh`
- Merge the `## Make Targets for DRAMSys Integration ##` part in Makefile
- Merge the `## Envirment Parameters for DRAMSys Integration ##` part in `sourceme.sh`
- Try to run `make drmasys_apply_patch`
	- if you encounter any errors, please try modify `core` & `pulp` submodules accroding to `add_dramsyslib_patches/gvsoc_core.patch` and `add_dramsyslib_patches/gvsoc_pulp.patch`, and generate new patches replacing the old one in `add_dramsyslib_patches`
- After all setps above, you can run `source dramsys_pushbutton_ETHenv.sh` or `source dramsys_pushbutton.sh` to build, run, and test the dramsys-integrated GVSoC on your branch.




## Citing

If you intend to use or reference GVSoC for an academic publication, please consider citing it:

```
@INPROCEEDINGS{9643828,
	author={Bruschi, Nazareno and Haugou, Germain and Tagliavini, Giuseppe and Conti, Francesco and Benini, Luca and Rossi, Davide},
	booktitle={2021 IEEE 39th International Conference on Computer Design (ICCD)},
	title={GVSoC: A Highly Configurable, Fast and Accurate Full-Platform Simulator for RISC-V based IoT Processors},
	year={2021},
	volume={},
	number={},
	pages={409-416},
	doi={10.1109/ICCD53106.2021.00071}}
```

## OS Requirements installation

These instructions were developed using a fresh Ubuntu 22.04 (Jammy Jellyfish).

The following packages needed to be installed:

~~~~~shell
sudo apt-get install -y build-essential git doxygen python3-pip libsdl2-dev curl cmake gtkwave libsndfile1-dev rsync autoconf automake texinfo libtool pkg-config libsdl2-ttf-dev
~~~~~


## Python requirements

Additional Python packages are needed and can be installed with the following commands from root folder:

```bash
git checkout snitch_cluster
git submodule update --init --recursive
pip3 install -r core/requirements.txt
pip3 install -r gapy/requirements.txt
```

## Installation

Get submodules and compile GVSoC with this command:

~~~~~shell
make all
~~~~~

It will by default build it for generic targets rv32 and rv64. You can build it for another target with this command:

~~~~~shell
make all TARGETS=pulp-open
~~~~~

On ETH network, please use this command to get the proper version of gcc and cmake:

~~~~~shell
CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make all TARGETS=pulp-open
~~~~~

You can also build snitch cluster for target using command:

~~~~~shell
CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make all TARGETS=pulp.snitch.snitch_cluster
~~~~~

The top generator of snitch cluster is under the directory pulp/pulp/snitch/snitch_cluster.py.

## Usage

The following example can be launched on pulp-open:

~~~~~shell
./install/bin/gvsoc --target=pulp-open --binary examples/pulp-open/hello image flash run
~~~~~

The another example can be launched on snitch-cluster with trace:

~~~~~shell
./install/bin/gvsoc --target=pulp.snitch.snitch_cluster --binary examples/snitch_cluster/name_of_binary_file --trace=.*:log.txt image flash run
~~~~~

For example, we want to run the simulation of binary file examples/snitch_cluster/axpy_fmadd.elf and read the trace related to the first integer core (/soc/pe0):

~~~~~shell
./install/bin/gvsoc --target=pulp.snitch.snitch_cluster --binary examples/snitch_cluster/axpy_fmadd.elf --trace=pe0:pe0.txt --trace-level=trace image flash run
~~~~~