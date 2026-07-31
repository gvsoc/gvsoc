#!/bin/bash

kvs=(128 256 512 1024 2048 4096)
for kv in "${kvs[@]}"; do
    file_path="experiments/TestSet1/FlashMLA_vs_FlatMLA/test_flatmla.py"
    screen -dmS test_flatmla_b128_sp8_kv${kv} bash -c "
        git clone git@github.com:gvsoc/gvsoc.git -b soft_hier_je test_flatmla_b128_sp8_kv${kv}
        cd test_flatmla_b128_sp8_kv${kv}
        sed -i \"257s/.*/    b  = 128/\" \"implementation/${file_path}\"
        sed -i \"258s/.*/    sp = 8/\" \"implementation/${file_path}\"
        sed -i \"259s/.*/    kv = ${kv}/\" \"implementation/${file_path}\"
        source sourceme.sh
        cd implementation
        arch=example/fp16_big_arch.py test=${file_path} make llm
    "
done
