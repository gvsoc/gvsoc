#!/bin/bash
set -e
set -x
source sourceme.sh

RedMule_list=(128 64 32 16 8 4)
TCDM_BANK_list=(512 256 128 64 32 16 8 4)
results_folder="result/test_bw_requirment"
mkdir -p ${results_folder}

for redmule_h in ${RedMule_list[@]}; do
    redmule_w=$((redmule_h / 4))
    sed -i "43s/.*/        self.redmule_ce_height       = ${redmule_h}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    sed -i "44s/.*/        self.redmule_ce_width        = ${redmule_w}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
    for tcdm_bank in ${TCDM_BANK_list[@]}; do
        sed -i "29s/.*/        self.cluster_tcdm_bank_nb    = ${tcdm_bank}/" pulp/pulp/chips/flex_cluster/flex_cluster_arch.py
        make iter run > ${results_folder}/RedMule_${redmule_h}x${redmule_w}_BW_${tcdm_bank}.log
    done
done