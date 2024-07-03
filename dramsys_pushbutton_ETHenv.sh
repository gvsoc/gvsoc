source sourceme.sh
CXX=g++-11.2.0 CC=gcc-11.2.0 make dramsys_preparation
pip3 install -r core/requirements.txt --user
pip3 install -r gapy/requirements.txt --user
pip3 install dataclasses --user
CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make TARGETS=pulp-open-ddr all
./install/bin/gvsoc --target=pulp-open-ddr --binary add_dramsyslib_patches/dma_dram_test.bin image flash run --trace=ddr
#example to run flex_cluster
make config
CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make TARGETS=pulp.chips.flex_cluster.flex_cluster all
make clean_sw && make sw
./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace-level=6 --trace=/chip/cluster_0/pe0/insn
