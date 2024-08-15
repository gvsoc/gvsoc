#!/bin/bash
set -e
set -x
source sourceme.sh

noc_bw=512
RedMuleBWFactor_list=(2 1)


for RedMuleBWFactor in ${RedMuleBWFactor_list[@]}; do

	##################################################################################
	#             -------------- OnChip 256MB HBM2e 64ch --------------              #
	##################################################################################

	results_folder="result/more_cluster_or_big_cluster/SystolicStream_NoC_${noc_bw}_OnChip_256MB_HBM_64ch_RedMuleBWFactor_${RedMuleBWFactor}x"
	sed -i "63s/.*/        self.noc_link_width          = ${noc_bw}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	mkdir -p ${results_folder}

	######################################
	#             Mesh 4x4               #
	######################################

	#configuration
	section="Mesh_4x4_RedMule_512x128"
	bank_width_original=32
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 512/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 128/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [4,0,0,4]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(1024,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log


	######################################
	#             Mesh 8x8               #
	######################################

	#configuration
	section="Mesh_8x8_RedMule_256x64"
	bank_width_original=16
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 256/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00400000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x00400000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [8,0,0,8]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(512,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log

	######################################
	#             Mesh 16x16             #
	######################################

	#configuration
	section="Mesh_16x16_RedMule_128x32"
	bank_width_original=8
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 128/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00100000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x00100000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [16,0,0,16]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(256,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log

	######################################
	#            Mesh 32x32              #
	######################################

	#configuration
	section="Mesh_32x32_RedMule_64x16"
	bank_width_original=4
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00040000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x00040000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 1/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [32,0,0,32]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(128,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log



	##################################################################################
	#             -------------- OnChip 64MB HBM2e 32ch --------------               #
	##################################################################################

	results_folder="result/more_cluster_or_big_cluster/SystolicStream_NoC_${noc_bw}_OnChip_64MB_HBM_32ch_RedMuleBWFactor_${RedMuleBWFactor}x"
	sed -i "63s/.*/        self.noc_link_width          = ${noc_bw}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	mkdir -p ${results_folder}

	######################################
	#             Mesh 2x2               #
	######################################

	#configuration
	section="Mesh_2x2_RedMule_512x128"
	bank_width_original=32
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 512/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 128/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x01000000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [2,0,0,2]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(1024,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log


	######################################
	#             Mesh 4x4               #
	######################################

	#configuration
	section="Mesh_4x4_RedMule_256x64"
	bank_width_original=16
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 256/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00400000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x00400000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 4/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [4,0,0,4]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(512,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log

	######################################
	#             Mesh 8x8               #
	######################################

	#configuration
	section="Mesh_8x8_RedMule_128x32"
	bank_width_original=8
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 128/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00100000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x00100000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 2/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [8,0,0,8]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(256,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log

	######################################
	#            Mesh 16x16              #
	######################################

	#configuration
	section="Mesh_16x16_RedMule_64x16"
	bank_width_original=4
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 64/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 16/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00040000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x00040000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 1/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [16,0,0,16]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(128,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log


	######################################
	#            Mesh 32x32              #
	######################################

	#configuration
	section="Mesh_32x32_RedMule_32x8"
	bank_width_original=2
	bank_width=$((bank_width_original * RedMuleBWFactor))
	sed -i "43s/.*/        self.redmule_ce_height       = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "44s/.*/        self.redmule_ce_width        = 8/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "28s/.*/        self.cluster_tcdm_bank_width = ${bank_width}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "24s/.*/        self.num_cluster_x           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "25s/.*/        self.num_cluster_y           = 32/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "32s/.*/        self.cluster_tcdm_size       = 0x00010000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "57s/.*/        self.hbm_node_interleave     = 0x00010000/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "58s/.*/        self.num_hbm_ch_per_node     = 1/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "59s/.*/        self.hbm_placement           = [16,0,0,16]/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
	sed -i "37s/.*/    llm_mlp_inter_cluster_matmul_systolic(64,2,ARCH_NUM_CLUSTER_X);/" add_dramsyslib_patches/flex_cluster_sdk/test/src/test.c

	#simulation
	make iter
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule > ${results_folder}/${section}_RedMule.log
	timeout 43200 ./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/idma > ${results_folder}/${section}_iDMA.log

done
