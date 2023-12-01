# GVSOC

GVSoC is the PULP chips simulator that is natively included in the Pulp SDK and is described and evaluated fully in Bruschi et al. [\[arXiv:2201.08166v1\]](https://arxiv.org/abs/2201.08166).

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
git submodule update --init --recursive -j8
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
