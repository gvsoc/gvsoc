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

For example, we want to run the simulation of binary file examples/snitch_cluster/apps/axpy/axpy_fmadd.elf and read the trace related to the first integer core (/soc/pe0):

~~~~~shell
./install/bin/gvsoc --target=pulp.snitch.snitch_cluster --binary examples/snitch_cluster/apps/axpy/axpy_fmadd.elf --trace=pe0:pe0.txt --trace-level=trace image flash run
~~~~~

## Extensions

Snitch cluster model can work with and without hardware extensions, e.g. frep, ssr and iDMA. 

In a core complex, the integer core and floating-point subsystem work in the pseudo dual issue mechanism. Each basic core complex is composed of a integer core and a private floating-point subsystem. We can also check the trace of the first subsystem (/soc/fp_ss0):

~~~~~shell
./install/bin/gvsoc --target=pulp.snitch.snitch_cluster --binary examples/snitch_cluster/apps/axpy/axpy_fmadd.elf --trace=fp_ss0:fp_ss0.txt --trace-level=trace image flash run
~~~~~

If you want to activate the fpu sequencer and test its functionality, the flag Xfrep in /pulp/pulp/snitch/snitch_cluster.py is set to 1 and the binary file axpy_frep.elf can be used. A sequencer is added in a core complex. The following command observes the trace of the first fpu sequencer (/soc/fpu_sequencer0):

~~~~~shell
./install/bin/gvsoc --target=pulp.snitch.snitch_cluster --binary examples/snitch_cluster/apps/axpy/axpy_frep.elf --trace=fpu_sequencer0:fpu_sequencer0.txt --trace-level=trace image flash run
~~~~~

## Profiler

It would be useful to investigate where execution time is being spent and improve the simulation speed. We can use the profiler perf to determine the bottelneck function.

Here are a few commands:

Measures and reports summary statistics (total number of cycles and instructions executed, cache accesses and misses, branch misprediction percentage). 

~~~~~shell
perf stat ./install/bin/gvsoc --target=pulp.snitch.snitch_cluster --binary examples/snitch_cluster/name_of_binary_file image flash run
~~~~~

Regularly samples the program (by default: ∼1000 Hz) and records events to a file (perf.data). The defalut event is cycles. The supported event list is included in perf list.

~~~~~shell
perf record ./install/bin/gvsoc --target=pulp.snitch.snitch_cluster --binary examples/snitch_cluster/name_of_binary_file image flash run
~~~~~

We can choose to keep track of branch misses by enabling the event. This helps us check which function inducing the most branch misses and the number of misses is probably reduced by adding likely()/unlikely() in branch prediction.

~~~~~shell
perf record -e branch-misses ./install/bin/gvsoc --target=pulp.snitch.snitch_cluster --binary examples/snitch_cluster/name_of_binary_file image flash run
~~~~~

Reads the samples and generates a table showing where the cycles were spent. This is presented as a call tree (i.e. if f() calls g() they are nested in one another) and is sorted descending according to number of samples and grouped by function and module.

~~~~~shell
perf report
~~~~~

Add a command flag if we only want to investigate performance related to thread gvsoc_launcher.

~~~~~shell
perf report --comms gvsoc_launcher
~~~~~

If you are interested in a more detailed view, this shows a disassembly intermingled with the source and gives an idea of the time spent on a single statement or instruction.

~~~~~shell
perf annotate
~~~~~