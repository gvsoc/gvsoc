#!/bin/bash

impl="fa2"
attns=("D128_S1024" "D128_S2048" "D128_S4096" "D64_S1024" "D64_S2048" "D64_S4096")
for attn in "${attns[@]}"; do
    arch_path="experiments/FlashFlatSH/arch/fp16_big_arch.py"
    attn_path="experiments/FlashFlatSH/FlatvsFlash/${impl}/${attn}.py"
    screen -dmS flatflash_${impl}_${attn} bash -c "
        git clone git@github.com:gvsoc/gvsoc.git -b soft_hier_je flatflash_${impl}_${attn}
        cd flatflash_${impl}_${attn}
        source sourceme.sh
        cd implementation
        arch=${arch_path} attn=${attn_path} make faft-runf
    "
done
