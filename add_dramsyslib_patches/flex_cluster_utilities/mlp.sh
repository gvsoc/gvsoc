#!/bin/bash
set -e
set -x
source sourceme.sh





######################################
# Enable Inter-Cluster-Communication #
######################################

mesh_list=(4 8 16)
tile_list=(16 32 64 128 256 512 1024)


for dim in ${mesh_list[@]}; do
    sed -i "24s/.*/        self.num_cluster_x           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    sed -i "25s/.*/        self.num_cluster_y           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    sed -i "58s/.*/        self.hbm_placement           = [${dim},0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    results_folder="result/mlp/inter_cluster_comm_redmule16x4_NoC2048/Mesh_${dim}x${dim}"
    mkdir -p ${results_folder}
    make iter
    for tile in ${tile_list[@]}; do
        sed -i "24s/.*/    llm_mlp_inter_cluster_matmul(${tile},2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
        make rebuild_sw
        filename="Tile_${tile}.log"
        timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${filename}
    done
done




#######################################
# Disable Inter-Cluster-Communication #
#######################################

# mesh_list=(4 8 16)
# tile_list=(16 32 64 128 256 512 1024)


# for dim in ${mesh_list[@]}; do
#     sed -i "24s/.*/        self.num_cluster_x           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#     sed -i "25s/.*/        self.num_cluster_y           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#     sed -i "58s/.*/        self.hbm_placement           = [${dim},0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#     results_folder="result/mlp/no_cluster_comm_redmule16x4_NoC2048/Mesh_${dim}x${dim}"
#     mkdir -p ${results_folder}
#     make iter
#     for tile in ${tile_list[@]}; do
#         sed -i "26s/.*/    llm_mlp_test(${tile},2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
#         make rebuild_sw
#         filename="Tile_${tile}.log"
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${filename}
#     done
# done




#########################################
# Optimized Inter-Cluster-Communication #
#########################################


# mesh_list=(4 8 16)
# tile_list=(16 32 64 128 256 512 1024)


# for dim in ${mesh_list[@]}; do
#     sed -i "24s/.*/        self.num_cluster_x           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#     sed -i "25s/.*/        self.num_cluster_y           = ${dim}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#     sed -i "58s/.*/        self.hbm_placement           = [${dim},0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#     results_folder="result/mlp/hbm_optimized_inter_cluster_comm_redmule128x32_NoC2048/Mesh_${dim}x${dim}"
#     mkdir -p ${results_folder}
#     make iter
#     for tile in ${tile_list[@]}; do
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(${tile},2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c
#         make rebuild_sw
#         filename="Tile_${tile}.log"
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${filename}
#     done
# done
