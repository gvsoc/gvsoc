#!/bin/bash
set -e
set -x
source sourceme.sh

# NoC_BW_list=(1024 2048 4096 8192)
NoC_BW_list=(512)
RedMule_list=(512 256 128 64 32 16 8 4)

# 56MB #
# for noc_bw in ${NoC_BW_list[@]}; do

#     sed -i "63s/.*/        self.noc_link_width          = ${noc_bw}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#     results_folder="result/noc_bw_and_onchip_budget_limitation/NoC_${noc_bw}_OnChip_56MB_HBM_3200GBs"
#     mkdir -p ${results_folder}


#     #iteration
#     for redmule_h in ${RedMule_list[@]}; do

#         redmule_w=$((redmule_h / 4))
#         sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         ######################################
#         #             Mesh 2x2               #
#         ######################################

#         #configuration
#         section="Mesh_2x2"
#         sed -i "24s/.*/        self.num_cluster_x           = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00a00000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [2,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(1024,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log



#         ######################################
#         #             Mesh 4x4               #
#         ######################################

#         #configuration
#         section="Mesh_4x4"
#         sed -i "24s/.*/        self.num_cluster_x           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00280000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [4,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(512,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


#         ######################################
#         #             Mesh 8x8               #
#         ######################################

#         #configuration
#         section="Mesh_8x8"
#         sed -i "24s/.*/        self.num_cluster_x           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x000a0000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x00400000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [8,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(256,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log



#         ######################################
#         #             Mesh 16x16             #
#         ######################################

#         #configuration
#         section="Mesh_16x16"
#         sed -i "24s/.*/        self.num_cluster_x           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00028000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x00100000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [16,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(128,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


#         ######################################
#         #             Mesh 32x32             #
#         ######################################

#         #configuration
#         section="Mesh_32x32"
#         sed -i "24s/.*/        self.num_cluster_x           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x0000a000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x00040000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [32,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(64,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 86400 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


#         # ######################################
#         # #             Mesh 64x64             #
#         # ######################################

#         # #configuration
#         # section="Mesh_64x64"
#         # sed -i "24s/.*/        self.num_cluster_x           = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "25s/.*/        self.num_cluster_y           = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00002800/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "57s/.*/        self.hbm_node_interleave     = 0x00010000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "58s/.*/        self.num_hbm_ch_per_node     = 1/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "59s/.*/        self.hbm_placement           = [64,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(32,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         # #simulation
#         # make iter
#         # timeout 86400 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log

#     done

# done

# 16MB #
for noc_bw in ${NoC_BW_list[@]}; do

    sed -i "63s/.*/        self.noc_link_width          = ${noc_bw}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    results_folder="result/noc_bw_and_onchip_budget_limitation/NoC_${noc_bw}_OnChip_16MB_HBM_3200GBs"
    mkdir -p ${results_folder}


    #iteration
    for redmule_h in ${RedMule_list[@]}; do

        redmule_w=$((redmule_h / 4))
        sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        ######################################
        #             Mesh 2x2               #
        ######################################

        #configuration
        section="Mesh_2x2"
        sed -i "24s/.*/        self.num_cluster_x           = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/        self.num_cluster_y           = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00a00000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "57s/.*/        self.hbm_node_interleave     = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "58s/.*/        self.num_hbm_ch_per_node     = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "59s/.*/        self.hbm_placement           = [2,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(512,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

        #simulation
        make iter
        timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log



        ######################################
        #             Mesh 4x4               #
        ######################################

        #configuration
        section="Mesh_4x4"
        sed -i "24s/.*/        self.num_cluster_x           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/        self.num_cluster_y           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00280000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "57s/.*/        self.hbm_node_interleave     = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "58s/.*/        self.num_hbm_ch_per_node     = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "59s/.*/        self.hbm_placement           = [4,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(256,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

        #simulation
        make iter
        timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


        ######################################
        #             Mesh 8x8               #
        ######################################

        #configuration
        section="Mesh_8x8"
        sed -i "24s/.*/        self.num_cluster_x           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/        self.num_cluster_y           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "32s/.*/        self.cluster_tcdm_size       = 0x000a0000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "57s/.*/        self.hbm_node_interleave     = 0x00400000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "58s/.*/        self.num_hbm_ch_per_node     = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "59s/.*/        self.hbm_placement           = [8,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(128,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

        #simulation
        make iter
        timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log



        ######################################
        #             Mesh 16x16             #
        ######################################

        #configuration
        section="Mesh_16x16"
        sed -i "24s/.*/        self.num_cluster_x           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/        self.num_cluster_y           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00028000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "57s/.*/        self.hbm_node_interleave     = 0x00100000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "58s/.*/        self.num_hbm_ch_per_node     = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "59s/.*/        self.hbm_placement           = [16,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(64,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

        #simulation
        make iter
        timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


        ######################################
        #             Mesh 32x32             #
        ######################################

        #configuration
        section="Mesh_32x32"
        sed -i "24s/.*/        self.num_cluster_x           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/        self.num_cluster_y           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "32s/.*/        self.cluster_tcdm_size       = 0x0000a000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "57s/.*/        self.hbm_node_interleave     = 0x00040000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "58s/.*/        self.num_hbm_ch_per_node     = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "59s/.*/        self.hbm_placement           = [32,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(32,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

        #simulation
        make iter
        timeout 86400 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


        # ######################################
        # #             Mesh 64x64             #
        # ######################################

        # #configuration
        # section="Mesh_64x64"
        # sed -i "24s/.*/        self.num_cluster_x           = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        # sed -i "25s/.*/        self.num_cluster_y           = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        # sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00002800/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        # sed -i "57s/.*/        self.hbm_node_interleave     = 0x00010000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        # sed -i "58s/.*/        self.num_hbm_ch_per_node     = 1/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        # sed -i "59s/.*/        self.hbm_placement           = [64,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        # sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(32,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

        # #simulation
        # make iter
        # timeout 86400 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log

    done

done


# 224MB #
# for noc_bw in ${NoC_BW_list[@]}; do

#     sed -i "63s/.*/        self.noc_link_width          = ${noc_bw}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#     results_folder="result/noc_bw_and_onchip_budget_limitation/NoC_${noc_bw}_OnChip_224MB_HBM_3200GBs"
#     mkdir -p ${results_folder}


#     #iteration
#     for redmule_h in ${RedMule_list[@]}; do

#         redmule_w=$((redmule_h / 4))
#         sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         ######################################
#         #             Mesh 2x2               #
#         ######################################

#         #configuration
#         section="Mesh_2x2"
#         sed -i "24s/.*/        self.num_cluster_x           = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x02800000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x04000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [2,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(2048,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log



#         ######################################
#         #             Mesh 4x4               #
#         ######################################

#         #configuration
#         section="Mesh_4x4"
#         sed -i "24s/.*/        self.num_cluster_x           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00a00000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x04000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [4,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(1024,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


#         ######################################
#         #             Mesh 8x8               #
#         ######################################

#         #configuration
#         section="Mesh_8x8"
#         sed -i "24s/.*/        self.num_cluster_x           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00280000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [8,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(512,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log



#         ######################################
#         #             Mesh 16x16             #
#         ######################################

#         #configuration
#         section="Mesh_16x16"
#         sed -i "24s/.*/        self.num_cluster_x           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x000a0000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x00400000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [16,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(256,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


#         ######################################
#         #             Mesh 32x32             #
#         ######################################

#         #configuration
#         section="Mesh_32x32"
#         sed -i "24s/.*/        self.num_cluster_x           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/        self.num_cluster_y           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00028000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "57s/.*/        self.hbm_node_interleave     = 0x00100000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "58s/.*/        self.num_hbm_ch_per_node     = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "59s/.*/        self.hbm_placement           = [32,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(128,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         #simulation
#         make iter
#         timeout 86400 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log


#         # ######################################
#         # #             Mesh 64x64             #
#         # ######################################

#         # #configuration
#         # section="Mesh_64x64"
#         # sed -i "24s/.*/        self.num_cluster_x           = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "25s/.*/        self.num_cluster_y           = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00002800/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "57s/.*/        self.hbm_node_interleave     = 0x00010000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "58s/.*/        self.num_hbm_ch_per_node     = 1/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "59s/.*/        self.hbm_placement           = [64,0,0,0]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
#         # sed -i "25s/.*/    llm_mlp_inter_cluster_matmul_optimize_hbm(32,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

#         # #simulation
#         # make iter
#         # timeout 86400 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule_${redmule_h}x${redmule_w}.log

#     done

# done
