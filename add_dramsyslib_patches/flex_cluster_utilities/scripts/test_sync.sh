#!/bin/bash
set -e
set -x
source sourceme.sh


mesh_list=(4 8 16 32 64 128)
results_folder="result/test_sync"
mkdir -p ${results_folder}

for dim in ${mesh_list[@]}; do
    sed -i "24s/.*/        self.num_cluster_x           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    sed -i "25s/.*/        self.num_cluster_y           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    filename="Mesh_${dim}x${dim}.log"
    make iter
    timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/ctrl_registers > ${results_folder}/${filename}
done