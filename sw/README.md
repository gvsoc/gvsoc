# Installation and Simulation

This example synthesizes compilation and simulation process by GVSoC. The input is a C-written testbench for snitch cluster. The building path is under test directory. deps, math, runtime and snRuntime are dependent libraries. blas contains basic linear algebra tests, e.g. axpy and gemm.

~~~~~shell
cd test
~~~~~

GVSOC can be compiled with make with this command:

~~~~~shell
make gvsoc
~~~~~

Compiling and running the application (gemm test here) with this command:

~~~~~shell
make sw run
~~~~~

Running all whole process together with this command:

~~~~~shell
make all
~~~~~