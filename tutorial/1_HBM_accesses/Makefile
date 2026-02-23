hbm_pld:
	cd preload; python gen_preload1.py

hbm_l1: hbm_pld
	cfg=$(abspath config/arch.py) app=$(abspath applications/hbm_l1) pld=$(abspath preload/my_preload.elf) make -C ../../ hs run

l1_hbm: hbm_pld
	cfg=$(abspath config/arch.py) app=$(abspath applications/l1_hbm) pld=$(abspath preload/my_preload.elf) make -C ../../ hs run

hbm_dump: hbm_pld
	cfg=$(abspath config/arch.py) app=$(abspath applications/hbm_dump) pld=$(abspath preload/my_preload.elf) make -C ../../ hs run