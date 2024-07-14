#!/bin/bash
# set -e
set -x

dimension_list=(1024 512 256 128 64 32)
source sourceme.sh

results_folder="result/mlp/no_cluster_communication"
mkdir -p ${results_folder}

for dim in ${dimension_list[@]}; do
	sed -i "21s/.*/    llm_mlp_test(${dim});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
	filename="Tile_${dim}x${dim}.log"
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${filename}
done
