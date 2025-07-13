CUR_DIR := $(shell pwd)

dsv3cfg:
	@echo "Generating configuration files..."
	python3 ${CUR_DIR}/scripts/dsv3_config.py ${CUR_DIR}/config/dsv3.py ${CUR_DIR}/software/include/dsv3.h

hs:
	cfg=${CUR_DIR}/config/arch.py app=${CUR_DIR}/software make -C ../../../../ hs

sw:
	cfg=${CUR_DIR}/config/arch.py app=${CUR_DIR}/software make -C ../../../../ sw

run:
	cfg=${CUR_DIR}/config/arch.py app=${CUR_DIR}/software make -C ../../../../ run

runv:
	cfg=${CUR_DIR}/config/arch.py app=${CUR_DIR}/software make -C ../../../../ runv

rund:
	cfg=${CUR_DIR}/config/arch.py app=${CUR_DIR}/software make -C ../../../../ rund

pfto:
	make -C ../../../../ pfto