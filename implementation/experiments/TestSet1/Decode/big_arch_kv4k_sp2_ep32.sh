#!/bin/bash

bs=(1 2 4 8 16 32 64 128 256 512 1024 2048)
for b in "${bs[@]}"; do
    sed_b="143s/.*/    batch_size = ${b}/"
    sed_kv="144s/.*/    kv_cache_length = 4096/"
    sed_ep="145s/.*/    expert_parallelsim = 32/"
    sed_sp="146s/.*/    speculative_factor = 2/"
    file_path="experiments/TestSet1/Decode/dsv3_fp8_decode.py"
    screen -dmS dsv3_decode_kv4k_sp2_ep32_b${b} bash -c "
        git clone git@github.com:gvsoc/gvsoc.git -b soft_hier_je dsv3_decode_kv4k_sp2_ep32_b${b}
        cd dsv3_decode_kv4k_sp2_ep32_b${b}
        sed -i ${sed_b}  implementation/${file_path}
        sed -i ${sed_kv} implementation/${file_path}
        sed -i ${sed_ep} implementation/${file_path}
        sed -i ${sed_sp} implementation/${file_path}
        source sourceme.sh
        cd implementation
        arch=example/big_arch.py test=${file_path} make llm
    "
done
