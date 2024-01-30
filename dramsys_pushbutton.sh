source sourceme.sh
make dramsys_preparation
pip3 install -r core/requirements.txt
pip3 install -r gapy/requirements.txt
make TARGETS=pulp-open-ddr all
gvsoc --target=pulp-open-ddr --binary add_dramsyslib_patches/dma_dram_test.bin image flash run --trace=ddr