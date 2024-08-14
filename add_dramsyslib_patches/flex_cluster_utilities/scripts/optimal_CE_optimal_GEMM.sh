#!/bin/bash
set -e
set -x
source sourceme.sh

m_list=(16 32 64 128 256 512)
n_list=(16 32 64 128 256 512)
k_list=(16 32 64 128 256 512)

# ######################################
# #           RedMule 32x8             #
# ######################################
# redmule_h=32
# redmule_w=8
# results_folder="result/optimal_CE_optimal_GEMM/RedMule_${redmule_h}x${redmule_w}"
# mkdir -p ${results_folder}
# sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# make iter

# for m in ${m_list[@]}; do
# 	for n in ${n_list[@]}; do
# 		for k in ${k_list[@]}; do
# 			sed -i "29s/.*/        flex_redmule_set_M(${m});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "30s/.*/        flex_redmule_set_N(${n});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "31s/.*/        flex_redmule_set_K(${k});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			make rebuild_sw
# 			timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/M${m}_N${n}_K${k}.log
# 		done
# 	done
# done


# ######################################
# #           RedMule 16x16            #
# ######################################
# redmule_h=16
# redmule_w=16
# results_folder="result/optimal_CE_optimal_GEMM/RedMule_${redmule_h}x${redmule_w}"
# mkdir -p ${results_folder}
# sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# make iter

# for m in ${m_list[@]}; do
# 	for n in ${n_list[@]}; do
# 		for k in ${k_list[@]}; do
# 			sed -i "29s/.*/        flex_redmule_set_M(${m});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "30s/.*/        flex_redmule_set_N(${n});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "31s/.*/        flex_redmule_set_K(${k});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			make rebuild_sw
# 			timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/M${m}_N${n}_K${k}.log
# 		done
# 	done
# done


# ######################################
# #           RedMule 8x32            #
# ######################################
# redmule_h=8
# redmule_w=32
# results_folder="result/optimal_CE_optimal_GEMM/RedMule_${redmule_h}x${redmule_w}"
# mkdir -p ${results_folder}
# sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# make iter

# for m in ${m_list[@]}; do
# 	for n in ${n_list[@]}; do
# 		for k in ${k_list[@]}; do
# 			sed -i "29s/.*/        flex_redmule_set_M(${m});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "30s/.*/        flex_redmule_set_N(${n});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "31s/.*/        flex_redmule_set_K(${k});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			make rebuild_sw
# 			timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/M${m}_N${n}_K${k}.log
# 		done
# 	done
# done


# ######################################
# #           RedMule 64x4             #
# ######################################
# redmule_h=64
# redmule_w=4
# results_folder="result/optimal_CE_optimal_GEMM/RedMule_${redmule_h}x${redmule_w}"
# mkdir -p ${results_folder}
# sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# make iter

# for m in ${m_list[@]}; do
# 	for n in ${n_list[@]}; do
# 		for k in ${k_list[@]}; do
# 			sed -i "29s/.*/        flex_redmule_set_M(${m});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "30s/.*/        flex_redmule_set_N(${n});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "31s/.*/        flex_redmule_set_K(${k});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			make rebuild_sw
# 			timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/M${m}_N${n}_K${k}.log
# 		done
# 	done
# done


# ######################################
# #           RedMule 64x4             #
# ######################################
# redmule_h=4
# redmule_w=64
# results_folder="result/optimal_CE_optimal_GEMM/RedMule_${redmule_h}x${redmule_w}"
# mkdir -p ${results_folder}
# sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
# make iter

# for m in ${m_list[@]}; do
# 	for n in ${n_list[@]}; do
# 		for k in ${k_list[@]}; do
# 			sed -i "29s/.*/        flex_redmule_set_M(${m});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "30s/.*/        flex_redmule_set_N(${n});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			sed -i "31s/.*/        flex_redmule_set_K(${k});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
# 			make rebuild_sw
# 			timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/M${m}_N${n}_K${k}.log
# 		done
# 	done
# done


######################################
#          RedMule 128x32            #
######################################
redmule_h=128
redmule_w=32
results_folder="result/optimal_CE_optimal_GEMM/RedMule_${redmule_h}x${redmule_w}"
mkdir -p ${results_folder}
sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
make iter

for m in ${m_list[@]}; do
	for n in ${n_list[@]}; do
		for k in ${k_list[@]}; do
			sed -i "29s/.*/        flex_redmule_set_M(0, ${m});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			sed -i "30s/.*/        flex_redmule_set_N(0, ${n});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			sed -i "31s/.*/        flex_redmule_set_K(0, ${k});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			make rebuild_sw
			timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/M${m}_N${n}_K${k}.log
		done
	done
done


######################################
#          RedMule 64x64             #
######################################
redmule_h=64
redmule_w=64
results_folder="result/optimal_CE_optimal_GEMM/RedMule_${redmule_h}x${redmule_w}"
mkdir -p ${results_folder}
sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
make iter

for m in ${m_list[@]}; do
	for n in ${n_list[@]}; do
		for k in ${k_list[@]}; do
			sed -i "29s/.*/        flex_redmule_set_M(0, ${m});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			sed -i "30s/.*/        flex_redmule_set_N(0, ${n});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			sed -i "31s/.*/        flex_redmule_set_K(0, ${k});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			make rebuild_sw
			timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/M${m}_N${n}_K${k}.log
		done
	done
done

######################################
#          RedMule 32x128            #
######################################
redmule_h=32
redmule_w=128
results_folder="result/optimal_CE_optimal_GEMM/RedMule_${redmule_h}x${redmule_w}"
mkdir -p ${results_folder}
sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
make iter

for m in ${m_list[@]}; do
	for n in ${n_list[@]}; do
		for k in ${k_list[@]}; do
			sed -i "29s/.*/        flex_redmule_set_M(0, ${m});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			sed -i "30s/.*/        flex_redmule_set_N(0, ${n});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			sed -i "31s/.*/        flex_redmule_set_K(0, ${k});/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
			make rebuild_sw
			timeout 1800 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/M${m}_N${n}_K${k}.log
		done
	done
done
