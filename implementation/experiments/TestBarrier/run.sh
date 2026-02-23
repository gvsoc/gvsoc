#!/bin/bash

dim_list=(2 4 8 16 32 64)
results_folder="out/test_barrier"
mkdir -p ${results_folder}

for dim in "${dim_list[@]}"; do
	sed -i "3s/.*/        self.num_cluster_x = ${dim}/" experiments/TestBarrier/arch.py
	sed -i "4s/.*/        self.num_cluster_y = ${dim}/" experiments/TestBarrier/arch.py
	sed -i "31s/.*/        self.num_node_per_ctrl = ${dim}/" experiments/TestBarrier/arch.py
	sed -i "33s/.*/        self.hbm_node_aliase = ${dim}/" experiments/TestBarrier/arch.py
	cfg=$(realpath experiments/TestBarrier/arch.py) app=$(realpath sw/Other/BarrierScale) make -C ../ hs runq | tee ${results_folder}/mesh_${dim}x${dim}.txt
	make -C ../ clean
done
