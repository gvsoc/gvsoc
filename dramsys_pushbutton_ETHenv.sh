source sourceme.sh
CXX=g++-11.2.0 CC=gcc-11.2.0 make dramsys_preparation
pip3 install -r core/requirements.txt --user
pip3 install -r gapy/requirements.txt --user
pip3 install dataclasses --user
CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make TARGETS=pulp-open-ddr all
./install/bin/gvsoc --target=pulp-open-ddr --binary add_dramsyslib_patches/dma_dram_test.bin image flash run --trace=ddr