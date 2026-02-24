CUR_DIR := $(shell pwd)
SH_DIR := $(realpath ..)

ARCH_FILE ?= ${CUR_DIR}/config/arch/arch.py
ifdef arch
	ARCH_FILE = $(realpath $(arch))
endif

export PYTHONPATH := ${CUR_DIR}/scripts:$(PYTHONPATH)


hw:
	@echo "Compiling SoftHier HW & SW for attention ..."
	cfg=${ARCH_FILE} make -C ${SH_DIR} hw

pfto:
	@echo "Compiling SoftHier HW & SW for attention ..."
	make -C ${SH_DIR} pfto

kernel-clean: attn-clean gemm-clean norm-clean acti-clean
clean: kernel-clean

#####################
#   FlatAttention   #
#####################

ATTN_FILE ?= ${CUR_DIR}/config/kernels/attn.py
ifdef attn
	ATTN_FILE = $(realpath $(attn))
endif

attn-cfg:
	@echo "Generating configuration files for attention ..."
	python3 ${CUR_DIR}/scripts/kernels/attn_config.py ${ATTN_FILE} ${CUR_DIR}/sw/FlatAttention/include/attn.h

attn-pld:
	@echo "Generating preload files for attention ..."
	python3 ${CUR_DIR}/scripts/kernels/attn_preload.py \
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

attn-clean:
	@echo "clean up intermidiate files generated for attention ..."
	rm -f ${CUR_DIR}/sw/FlatAttention/include/attn.h ${CUR_DIR}/sw/FlatAttention/preload.elf ${CUR_DIR}/sw/FlatAttention/preload.dump ${CUR_DIR}/sw/FlatAttention/include/preload.h ${SH_DIR}/dump_*



#############
#   GEMM    #
#############

GEMM_FILE ?= ${CUR_DIR}/config/kernels/gemm.py
ifdef gemm
	GEMM_FILE = $(realpath $(gemm))
endif

gemm-cfg:
	@echo "Generating configuration files for GEMM ..."
	python3 ${CUR_DIR}/scripts/kernels/gemm_config.py ${GEMM_FILE} ${CUR_DIR}/sw/SummaGEMM/include/gemm.h

gemm-pld:
	@echo "Generating preload files for GEMM ..."
	python3 ${CUR_DIR}/scripts/kernels/gemm_preload.py \
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

gemm-clean:
	@echo "clean up intermidiate files generated for GEMM ..."
	rm -f ${CUR_DIR}/sw/SummaGEMM/include/gemm.h ${CUR_DIR}/sw/SummaGEMM/preload.elf ${CUR_DIR}/sw/SummaGEMM/preload.dump ${CUR_DIR}/sw/SummaGEMM/include/preload.h ${SH_DIR}/dump_*



######################
#   Normolization    #
######################

NORM_FILE ?= ${CUR_DIR}/config/kernels/norm.py
ifdef norm
	NORM_FILE = $(realpath $(norm))
endif

norm-cfg:
	@echo "Generating configuration files for Normolization ..."
	python3 ${CUR_DIR}/scripts/kernels/norm_config.py ${NORM_FILE} ${CUR_DIR}/sw/RMSNorm/include/norm.h

norm-pld:
	@echo "Generating preload files for Normolization ..."
	python3 ${CUR_DIR}/scripts/kernels/norm_preload.py \
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

norm-clean:
	@echo "clean up intermidiate files generated for Normolization ..."
	rm -f ${CUR_DIR}/sw/RMSNorm/include/norm.h ${CUR_DIR}/sw/RMSNorm/preload.elf ${CUR_DIR}/sw/RMSNorm/preload.dump ${CUR_DIR}/sw/RMSNorm/include/preload.h ${SH_DIR}/dump_*



###################
#   Activation    #
###################

ACTI_FILE ?= ${CUR_DIR}/config/kernels/acti.py
ifdef acti
	ACTI_FILE = $(realpath $(acti))
endif

acti-cfg:
	@echo "Generating configuration files for Activation ..."
	python3 ${CUR_DIR}/scripts/kernels/acti_config.py ${ACTI_FILE} ${CUR_DIR}/sw/Activation/include/acti.h

acti-pld:
	@echo "Generating preload files for Activation ..."
	python3 ${CUR_DIR}/scripts/kernels/acti_preload.py \
			${CUR_DIR}/sw/Activation/preload.elf \
			${CUR_DIR}/sw/Activation/include/preload.h \
			${ARCH_FILE} \
			${ACTI_FILE}

acti-hs:
	@echo "Compiling SoftHier HW & SW for Activation ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/Activation make -C ${SH_DIR} hs

acti-sw:
	@echo "Compiling SoftHier HW & SW for Activation ..."
	cfg=${ARCH_FILE} app=${CUR_DIR}/sw/Activation make -C ${SH_DIR} sw

acti-pre: acti-clean acti-cfg acti-pld acti-hs

acti-run: acti-pre
	@echo "SoftHier simulation for Activation ..."
	pld=${CUR_DIR}/sw/Activation/preload.elf make -C ${SH_DIR} run

acti-run%: acti-pre
	@echo "SoftHier simulation for Activation ..."
	pld=${CUR_DIR}/sw/Activation/preload.elf make -C ${SH_DIR} run$*

acti-clean:
	@echo "clean up intermidiate files generated for Activation ..."
	rm -f ${CUR_DIR}/sw/Activation/include/acti.h ${CUR_DIR}/sw/Activation/preload.elf ${CUR_DIR}/sw/Activation/preload.dump ${CUR_DIR}/sw/Activation/include/preload.h ${SH_DIR}/dump_*
