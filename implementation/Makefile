CUR_DIR := $(shell pwd)
SH_DIR := $(realpath ..)

ARCH_FILE ?= ${CUR_DIR}/config/arch.py
ifdef arch
	ARCH_FILE = $(realpath $(arch))
endif

ATTN_FILE ?= ${CUR_DIR}/config/attn.py
ifdef attn
	ATTN_FILE = $(realpath $(attn))
endif


hw:
	@echo "Compiling SoftHier HW & SW for attention ..."
	cfg=${ARCH_FILE} make -C ${SH_DIR} hw

pfto:
	@echo "Compiling SoftHier HW & SW for attention ..."
	make -C ${SH_DIR} pfto

attn-cfg:
	@echo "Generating configuration files for attention ..."
	python3 ${CUR_DIR}/scripts/attn_config.py ${ATTN_FILE} ${CUR_DIR}/sw/FlatAttention/include/attn.h

attn-pld:
	@echo "Generating preload files for attention ..."
	python3 ${CUR_DIR}/scripts/attn_preload.py \
			${CUR_DIR}/sw/FlatAttention/preload.elf \
			${CUR_DIR}/sw/FlatAttention/include/preload.h \
			${ARCH_FILE} \
			${ATTN_FILE}

attn-hs:
	@echo "Compiling SoftHier HW & SW for attention ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/FlatAttention make -C ${SH_DIR} hs

attn-pre: attn-clean attn-cfg attn-pld attn-hs

attn-run: attn-pre
	@echo "SoftHier simulation for attention ..."
	pld=${CUR_DIR}/sw/FlatAttention/preload.elf make -C ${SH_DIR} run

attn-run%: attn-pre
	@echo "SoftHier simulation for attention ..."
	pld=${CUR_DIR}/sw/FlatAttention/preload.elf make -C ${SH_DIR} run$*

attn-num:
	@echo "Numerical check for attnetion ..."
	python3 ${CUR_DIR}/scripts/attn_numerical_check.py ${SH_DIR}/dump_0 ${ATTN_FILE}

attn-clean:
	@echo "clean up intermidiate files generated for attention ..."
	rm -f ${CUR_DIR}/sw/FlatAttention/include/attn.h ${CUR_DIR}/sw/FlatAttention/preload.elf ${CUR_DIR}/sw/FlatAttention/preload.dump ${CUR_DIR}/sw/FlatAttention/include/preload.h ${SH_DIR}/dump_*


