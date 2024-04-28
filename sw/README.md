# Run GVSoC with Snitch Cluster

This example synthesizes compilation and simulation process by GVSoC. The input is a C-written testbench for Snitch Cluster. You can benchmark software and run simulations for the Snitch Cluster architecture.

## Getting Start

### Installation

Clone the repository:

~~~~~shell
git clone https://github.com/gvsoc/gvsoc.git
~~~~~

Clone its submodules:

~~~~~shell
git checkout snitch_cluster_test
git submodule update --init --recursive
~~~~~

## Python requirements

Additional Python packages are needed and can be installed with the following commands from root folder:

```bash
source sourceme.sh
pip3 install -r core/requirements.txt
pip3 install -r gapy/requirements.txt
```

# Building the software and running a simulation

Then, go to the simulation environment for Snitch Cluster, run the command,

~~~~~shell
cd sw
~~~~~

In `sw` directory, `deps`, `math`, `runtime` and `snRuntime` are dependent libraries for benchmark. `blas` contains basic linear algebra tests to help you get a quick start, e.g. `axpy` and `gemm`. In the following part, the working directory is `tests`. 

~~~~~shell
cd tests
~~~~~

GVSOC can be compiled with make with this command and it will build for target -- `snitch_cluster_single` a single snitch cluster with eight computation cores, one DMA core and corresponding memory, interconnection components. The top generator of snitch cluster is under the directory `pulp/pulp/snitch/snitch_cluster_single.py`. Run the command:

~~~~~shell
make gvsoc
~~~~~

For example, we need to run `gemm` test. This testbench has already been prepared in `blas/gemm/src`. Compiling the application with this command:

~~~~~shell
make sw
~~~~~

You will see the ELF binary generated as `tests/build/test`. Running the application on Snitch Cluster model with this command:

~~~~~shell
make run
~~~~~

Trace helps us a lot in debugging. It shows the instruction flows and computation results on each component. If we want the test to be launched on Snitch Cluster with trace, run this command:

~~~~~shell
make run-trace
~~~~~

The trace can be found in `tests/log.txt`. This trace file contains the execution details of all submodules.

If you want to run the whole process together, use the following command:

~~~~~shell
make all
~~~~~