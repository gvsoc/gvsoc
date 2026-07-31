#!/bin/bash

hds=(64 128)
qs=(512 1024 2048 4096)
for hd in "${hds[@]}"; do
    for q in "${qs[@]}"; do
        file_path="experiments/TestSet1/FlashAttn_vs_FlatAttn/test_prefill_flatattn.py"
        screen -dmS prefill_mha_hd${hd}_q${q} bash -c "
            git clone git@github.com:gvsoc/gvsoc.git -b soft_hier_je prefill_mha_hd${hd}_q${q}
            cd prefill_mha_hd${hd}_q${q}
            sed -i \"240s/.*/    b   = 32/\" \"implementation/${file_path}\"
            sed -i \"241s/.*/    qs  = ${q}/\" \"implementation/${file_path}\"
            sed -i \"242s/.*/    hd  = ${hd}/\" \"implementation/${file_path}\"
            sed -i \"243s/.*/    nh  = 32/\" \"implementation/${file_path}\"
            sed -i \"244s/.*/    nhg = 32/\" \"implementation/${file_path}\"
            source sourceme.sh
            cd implementation
            arch=example/fp16_big_arch.py test=${file_path} make llm
        "
    done
done
