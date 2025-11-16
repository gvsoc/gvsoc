#!/bin/bash

sps=(1 2 4)
kvs=(512 1024 2048 4096)
for sp in "${sps[@]}"; do
    for kv in "${kvs[@]}"; do
        file_path="experiments/TestSet1/FlashAttn_vs_FlatAttn/test_decode_flatattn.py"
        screen -dmS decode_gqa_sp${sp}_kv${kv} bash -c "
            git clone git@github.com:gvsoc/gvsoc.git -b soft_hier_je decode_gqa_sp${sp}_kv${kv}
            cd decode_gqa_sp${sp}_kv${kv}
            sed -i \"241s/.*/    b   = 128/\" \"implementation/${file_path}\"
            sed -i \"242s/.*/    sp  = ${sp}/\" \"implementation/${file_path}\"
            sed -i \"243s/.*/    kv  = ${kv}/\" \"implementation/${file_path}\"
            sed -i \"244s/.*/    hd  = 128/\" \"implementation/${file_path}\"
            sed -i \"245s/.*/    nh  = 32/\" \"implementation/${file_path}\"
            sed -i \"246s/.*/    nhg = 2/\" \"implementation/${file_path}\"
            source sourceme.sh
            cd implementation
            arch=example/fp16_big_arch.py test=${file_path} make llm
        "
    done
done
