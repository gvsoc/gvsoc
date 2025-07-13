#!/bin/bash

sequence_length_FP16=(1 2 4 8 16 32 64 128 256 512 1024 2048 4096)

mkdir -p result/test_kr

for seqlen in "${sequence_length_FP16[@]}"; do
	sed -i "10s/.*/#define SEQ_LEN ${seqlen}/" software/main.c
	mkdir -p result/test_kr/FP16_Seq${seqlen}
	make sw runv pfto
	mv ../../../../sw_build/analyze_trace.txt result/test_kr/FP16_Seq${seqlen}
	mv ../../../../sw_build/perfetto.json result/test_kr/FP16_Seq${seqlen}
done