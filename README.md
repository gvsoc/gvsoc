# GVSoC

GVSoC is the PULP chips simulator that is natively included in the Pulp SDK and is described and evaluated fully in Bruschi et al. [\[arXiv:2201.08166v1\]](https://arxiv.org/abs/2201.08166).


## GVSoC documentations

The user documentation, focusing on how to use GVSOC, is available [here](https://gvsoc.readthedocs.io/en/latest/).

The developer documentation, focusing on how to develop models or extend GVSOC is available [here] (https://gvsoc-developer.readthedocs.io/en/latest/).

## GVSoC Tutorial

If you want to learn more about GVSoC, get started through the tutorial available [here](https://gvsoc-developer.readthedocs.io/en/latest/tutorials.html). This tutorial provides hands-on practice on building systems on GVSoC and extracting the performance results.


## OS Requirements installation

These instructions were developed using a fresh Ubuntu 22.04 (Jammy Jellyfish).

The following packages needed to be installed:

~~~~~shell
sudo apt-get install -y build-essential git doxygen python3-pip libsdl2-dev curl cmake gtkwave libsndfile1-dev rsync autoconf automake texinfo libtool pkg-config libsdl2-ttf-dev wget sphinx-build doxygen libdwarf-dev
~~~~~

These are the packages neded on a Fedora:

~~~~~shell
sudo dnf install -y make gcc cmake ninja-build.x86_64 g++ pip lz4-devel ccache glibc-devel.i686 zlib-ng-compat-devel.i686 SDL2 SDL2-devel SDL2_ttf-devel.x86_64 SDL2_image-devel.x86_64 wget2 sphinx-build doxygen libdwarf-devel
~~~~~

Please also check any README.md in the submodules for target-specific requirements, like for example pulp/README.md.

### libdwarf

GVSoC needs `libdwarf` at build and run time (the ISS and GUI resolve trace
symbols through it). It is available by default on most machines (on Ubuntu it
is provided by `libdwarf-dev`, on Fedora by `libdwarf-devel`), so usually
nothing special is required.

On hosts where `libdwarf` is missing and cannot be installed system-wide (for
example shared compute clusters where you have no root access), run the `deps`
target once. It downloads and builds `libdwarf` from source and installs it into
the SDK install folder:

~~~~~shell
make deps
~~~~~

The build adds the install folder's `lib/pkgconfig` to `PKG_CONFIG_PATH` (after
the regular dirs), so once `make deps` has run this private copy is discovered
at build time; otherwise the build uses the system `libdwarf`. The install
folder's `lib` is also baked into the binaries' RPATH, so the private copy is
found at run time without any extra setup (no need to export
`LD_LIBRARY_PATH`). You only need to run `make deps` once (or after `make
clean`), before building.

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
make all TARGETS=<target>
~~~~~

<target> should indicate the target for which GVSoC must be build. This can for example be generic targets rv32 or rv64. 

On ETH network, please use this command to get the proper version of gcc and cmake:

~~~~~shell
CXX=g++-14.2.0 CC=gcc-14.2.0 CMAKE=cmake-3.18.1 make all TARGETS=pulp-open
~~~~~

The ETH machines do not ship `libdwarf`, and it cannot be installed
system-wide, so you must build it into the SDK install folder once with the
`deps` target before building (see the [libdwarf](#libdwarf) section above):

~~~~~shell
CXX=g++-14.2.0 CC=gcc-14.2.0 CMAKE=cmake-3.18.1 make deps
CXX=g++-14.2.0 CC=gcc-14.2.0 CMAKE=cmake-3.18.1 make all TARGETS=pulp-open
~~~~~

## Usage

The following example can be launched on pulp-open:

~~~~~shell
./install/bin/gvsoc --target=pulp-open --binary examples/pulp-open/hello image flash run
~~~~~

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

## Using GVSoC with DRAMsys

If you want to use DRAMsys with GVSoC follow the steps mentioned in [DRAMsys.md](./DRAMSys.md)
