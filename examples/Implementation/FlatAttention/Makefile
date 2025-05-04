# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Chi Zhang, ETH Zurich

attn_file ?= configs/attn.py
ifdef attn
	attn_file = $(attn)
endif

arch_file ?= configs/arch.py
ifdef arch
	arch_file = $(arch)
endif

CHECK_VAR := $(shell \
    grep "flatten_numerical_check" $(attn_file) \
    | sed -n 's/.*flatten_numerical_check *= *\([0-9]*\).*/\1/p' \
)

init:
	mkdir -p build
	cd build; git clone https://github.com/gvsoc/gvsoc.git -b soft_hier_release soft_hier_release
	cd build/soft_hier_release; git reset --hard 2768cacac8efe7d3dfa5fd6f23085a88a4ff9689;

clean:
	rm -rf build
	rm -rf configs/__pycache__/

assert:
	python scripts/assertion.py $(realpath $(attn_file)) $(realpath $(arch_file))

gen_pld: assert
	rm -rf build/preload/
	mkdir -p build/preload/golden
	python scripts/gen_preload.py build/preload/preload.elf build/preload/golden $(realpath $(attn_file)) $(realpath $(arch_file))

check_results:
	python scripts/check_results.py build/soft_hier_release/dump_0 build/preload/golden $(realpath $(attn_file)) $(realpath $(arch_file))

sw_cfg: assert
	rm -rf build/sw
	cp -rf software build/sw
	python scripts/sw_config.py $(realpath $(attn_file)) build/sw/include/FlatAttentionConfig.h

hs: sw_cfg
	cfg=$(realpath $(arch_file)) \
	app=$(realpath build/sw) \
	make -C build/soft_hier_release hs

run: hs
	@if [ "$(CHECK_VAR)" != "0" ] && [ "$(CHECK_VAR)" != "" ]; then \
		echo "I want to enable flatten_numerical_check"; \
		make gen_pld; \
		pld=$(realpath build/preload/preload.elf) make -C build/soft_hier_release runv; \
		make check_results; \
	else \
		make -C build/soft_hier_release runv; \
	fi

run_simple_trace: hs
	@if [ "$(CHECK_VAR)" != "0" ] && [ "$(CHECK_VAR)" != "" ]; then \
		echo "I want to enable flatten_numerical_check"; \
		make gen_pld; \
		pld=$(realpath build/preload/preload.elf) make -C build/soft_hier_release runv_simple; \
		make check_results; \
	else \
		make -C build/soft_hier_release runv_simple; \
	fi

pfto: build/soft_hier_release/sw_build/analyze_trace.txt
	rm -rf build/perfetto
	mkdir -p build/perfetto
	make -C build/soft_hier_release pfto
	mv build/soft_hier_release/sw_build/perfetto.json build/perfetto

