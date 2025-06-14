config_folder=Fig3_D128_S4096_FlatAsync
make clean
make -C build/soft_hier_release/ clean
arch=paper_examples/${config_folder}/arch.py attn=paper_examples/${config_folder}/attn.py make gen_pld
arch=paper_examples/${config_folder}/arch.py attn=paper_examples/${config_folder}/attn.py make hs
pld=$(pwd)/build/preload/preload.elf make -C build/soft_hier_release/ runv_simple
arch=paper_examples/${config_folder}/arch.py attn=paper_examples/${config_folder}/attn.py make check_results
