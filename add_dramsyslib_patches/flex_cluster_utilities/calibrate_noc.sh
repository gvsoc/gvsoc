#!/bin/bash
# set -e
set -x

noc_dim_list=(1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32)
source sourceme.sh

####################
# Dialog to Dialog #
####################
results_folder="result/dialog"
mkdir -p ${results_folder}
sed -i "14s/.*/        flex_dma_pattern_dialog_to_dialog(0, size, size);/" add_dramsyslib_patches/flex_cluster_pdk/test/src/test.c

for dim in ${noc_dim_list[@]}; do
    sed -i "24s/.*/        self.num_cluster_x           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    sed -i "25s/.*/        self.num_cluster_y           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    filename="NoC_${dim}x${dim}.log"
    make iter
    timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace-level=6 --trace=/chip/ctrl_registers > ${results_folder}/${filename}
done

####################
#    All to One    #
####################
results_folder="result/all_to_one"
mkdir -p ${results_folder}
sed -i "14s/.*/        flex_dma_pattern_all_to_one(0, size, size);/" add_dramsyslib_patches/flex_cluster_pdk/test/src/test.c

for dim in ${noc_dim_list[@]}; do
    sed -i "24s/.*/        self.num_cluster_x           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    sed -i "25s/.*/        self.num_cluster_y           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    filename="NoC_${dim}x${dim}.log"
    make iter
    timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace-level=6 --trace=/chip/ctrl_registers > ${results_folder}/${filename}
done

####################
# Round Shift Left #
####################
results_folder="result/round_shift_left"
mkdir -p ${results_folder}
sed -i "14s/.*/        flex_dma_pattern_round_shift_left(0, size, size);/" add_dramsyslib_patches/flex_cluster_pdk/test/src/test.c

for dim in ${noc_dim_list[@]}; do
    sed -i "24s/.*/        self.num_cluster_x           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    sed -i "25s/.*/        self.num_cluster_y           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    filename="NoC_${dim}x${dim}.log"
    make iter
    timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace-level=6 --trace=/chip/ctrl_registers > ${results_folder}/${filename}
done
