#!/bin/bash

bs=(1 2 4 8 16 32 64 128 256 512 1024 2048)
for b in "${bs[@]}"; do
    file_path="experiments/TestSet1/Decode/dsv3_fp8_decode.py"
    screen -dmS dsv3_decode_kv4k_sp2_ep32_b${b} bash -c "
        git clone git@github.com:gvsoc/gvsoc.git -b soft_hier_je dsv3_decode_kv4k_sp2_ep32_b${b}
        cd dsv3_decode_kv4k_sp2_ep32_b${b}
        sed -i \"143s/.*/    batch_size = ${b}/\" \"implementation/${file_path}\"
        sed -i \"144s/.*/    kv_cache_length = 4096/\" \"implementation/${file_path}\"
        sed -i \"145s/.*/    expert_parallelsim = 32/\" \"implementation/${file_path}\"
        sed -i \"146s/.*/    speculative_factor = 2/\" \"implementation/${file_path}\"
        source sourceme.sh
        cd implementation
        arch=example/big_arch.py test=${file_path} make llm
    "
done
