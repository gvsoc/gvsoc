#!/bin/bash

test_attn="attn_D128_S1024"
stage_list=(2 4 8 16 32 64 128 256)
results_folder="out/test_swcoll/${test_attn}"
mkdir -p ${results_folder}

for stage in "${stage_list[@]}"; do
	sed -i "45s/.*/        self.flatten_swcoll_pipeline = ${stage}/" experiments/TestSWColl/large_test/${test_attn}.py
	arch=experiments/TestSWColl/large_test/arch.py attn=experiments/TestSWColl/large_test/${test_attn}.py make attn-runv_simple
	cp ../sw_build/analyze_trace.txt ${results_folder}/pipeline_stage_${stage}.txt
	make kernel-clean
done
