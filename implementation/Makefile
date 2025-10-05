CUR_DIR := $(shell pwd)
SH_DIR := $(realpath ..)

ARCH_FILE ?= ${CUR_DIR}/config/arch.py
ifdef arch
	ARCH_FILE = $(realpath $(arch))
endif

export PYTHONPATH := ${CUR_DIR}/script:$(PYTHONPATH)


hw:
	@echo "Compiling SoftHier HW & SW for attention ..."
	cfg=${ARCH_FILE} make -C ${SH_DIR} hw

pfto:
	@echo "Compiling SoftHier HW & SW for attention ..."
	make -C ${SH_DIR} pfto

#####################
#   FlatAttention   #
#####################

ATTN_FILE ?= ${CUR_DIR}/config/attn.py
ifdef attn
	ATTN_FILE = $(realpath $(attn))
endif

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

attn-sw:
	@echo "Compiling SoftHier HW & SW for attention ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/FlatAttention make -C ${SH_DIR} sw

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



#############
#   GEMM    #
#############

GEMM_FILE ?= ${CUR_DIR}/config/gemm.py
ifdef gemm
	GEMM_FILE = $(realpath $(gemm))
endif

gemm-cfg:
	@echo "Generating configuration files for GEMM ..."
	python3 ${CUR_DIR}/scripts/gemm_config.py ${GEMM_FILE} ${CUR_DIR}/sw/SummaGEMM/include/gemm.h

gemm-pld:
	@echo "Generating preload files for GEMM ..."
	python3 ${CUR_DIR}/scripts/gemm_preload.py \
			${CUR_DIR}/sw/SummaGEMM/preload.elf \
			${CUR_DIR}/sw/SummaGEMM/include/preload.h \
			${ARCH_FILE} \
			${GEMM_FILE}

gemm-hs:
	@echo "Compiling SoftHier HW & SW for GEMM ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/SummaGEMM make -C ${SH_DIR} hs

gemm-sw:
	@echo "Compiling SoftHier HW & SW for GEMM ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/SummaGEMM make -C ${SH_DIR} sw

gemm-pre: gemm-clean gemm-cfg gemm-pld gemm-hs

gemm-run: gemm-pre
	@echo "SoftHier simulation for GEMM ..."
	pld=${CUR_DIR}/sw/SummaGEMM/preload.elf make -C ${SH_DIR} run

gemm-run%: gemm-pre
	@echo "SoftHier simulation for GEMM ..."
	pld=${CUR_DIR}/sw/SummaGEMM/preload.elf make -C ${SH_DIR} run$*

gemm-num:
	@echo "Numerical check for GEMM ..."
	python3 ${CUR_DIR}/scripts/gemm_numerical_check.py ${SH_DIR}/dump_0 ${GEMM_FILE}

gemm-clean:
	@echo "clean up intermidiate files generated for GEMM ..."
	rm -f ${CUR_DIR}/sw/SummaGEMM/include/gemm.h ${CUR_DIR}/sw/SummaGEMM/preload.elf ${CUR_DIR}/sw/SummaGEMM/preload.dump ${CUR_DIR}/sw/SummaGEMM/include/preload.h ${SH_DIR}/dump_*



######################
#   Normolization    #
######################

NORM_FILE ?= ${CUR_DIR}/config/norm.py
ifdef norm
	NORM_FILE = $(realpath $(norm))
endif

norm-cfg:
	@echo "Generating configuration files for Normolization ..."
	python3 ${CUR_DIR}/scripts/norm_config.py ${NORM_FILE} ${CUR_DIR}/sw/RMSNorm/include/norm.h

norm-pld:
	@echo "Generating preload files for Normolization ..."
	python3 ${CUR_DIR}/scripts/norm_preload.py \
			${CUR_DIR}/sw/RMSNorm/preload.elf \
			${CUR_DIR}/sw/RMSNorm/include/preload.h \
			${ARCH_FILE} \
			${NORM_FILE}

norm-hs:
	@echo "Compiling SoftHier HW & SW for Normolization ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/RMSNorm make -C ${SH_DIR} hs

norm-sw:
	@echo "Compiling SoftHier HW & SW for Normolization ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/RMSNorm make -C ${SH_DIR} sw

norm-pre: norm-clean norm-cfg norm-pld norm-hs

norm-run: norm-pre
	@echo "SoftHier simulation for Normolization ..."
	pld=${CUR_DIR}/sw/RMSNorm/preload.elf make -C ${SH_DIR} run

norm-run%: norm-pre
	@echo "SoftHier simulation for Normolization ..."
	pld=${CUR_DIR}/sw/RMSNorm/preload.elf make -C ${SH_DIR} run$*

norm-num:
	@echo "Numerical check for Normolization ..."
	python3 ${CUR_DIR}/scripts/norm_numerical_check.py ${SH_DIR}/dump_0 ${NORM_FILE}

norm-clean:
	@echo "clean up intermidiate files generated for Normolization ..."
	rm -f ${CUR_DIR}/sw/RMSNorm/include/norm.h ${CUR_DIR}/sw/RMSNorm/preload.elf ${CUR_DIR}/sw/RMSNorm/preload.dump ${CUR_DIR}/sw/RMSNorm/include/preload.h ${SH_DIR}/dump_*



#############
#   RoPE    #
#############

ROPE_FILE ?= ${CUR_DIR}/config/rope.py
ifdef rope
	ROPE_FILE = $(realpath $(rope))
endif

rope-cfg:
	@echo "Generating configuration files for RoPE ..."
	python3 ${CUR_DIR}/scripts/rope_config.py ${ROPE_FILE} ${CUR_DIR}/sw/RoPE/include/rope.h

rope-pld:
	@echo "Generating preload files for RoPE ..."
	python3 ${CUR_DIR}/scripts/rope_preload.py \
			${CUR_DIR}/sw/RoPE/preload.elf \
			${CUR_DIR}/sw/RoPE/include/preload.h \
			${ARCH_FILE} \
			${ROPE_FILE}

rope-hs:
	@echo "Compiling SoftHier HW & SW for RoPE ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/RoPE make -C ${SH_DIR} hs

rope-sw:
	@echo "Compiling SoftHier HW & SW for RoPE ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/RoPE make -C ${SH_DIR} sw

rope-pre: rope-clean rope-cfg rope-pld rope-hs

rope-run: rope-pre
	@echo "SoftHier simulation for RoPE ..."
	pld=${CUR_DIR}/sw/RoPE/preload.elf make -C ${SH_DIR} run

rope-run%: rope-pre
	@echo "SoftHier simulation for RoPE ..."
	pld=${CUR_DIR}/sw/RoPE/preload.elf make -C ${SH_DIR} run$*

rope-num:
	@echo "Numerical check for RoPE ..."
	python3 ${CUR_DIR}/scripts/rope_numerical_check.py ${SH_DIR}/dump_0 ${ROPE_FILE}

rope-clean:
	@echo "clean up intermidiate files generated for RoPE ..."
	rm -f ${CUR_DIR}/sw/RoPE/include/rope.h ${CUR_DIR}/sw/RoPE/preload.elf ${CUR_DIR}/sw/RoPE/preload.dump ${CUR_DIR}/sw/RoPE/include/preload.h ${SH_DIR}/dump_*

