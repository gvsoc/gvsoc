CMAKE_FLAGS ?= -j 6
CMAKE ?= cmake

TARGETS ?= rv32;rv64

export PATH:=$(CURDIR)/gapy/bin:$(PATH)

all: checkout build

checkout:
	git submodule update --recursive --init

.PHONY: build

build:
	# Change directory to curdir to avoid issue with symbolic links
	cd $(CURDIR) && $(CMAKE) -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DCMAKE_INSTALL_PREFIX=install \
		-DGVSOC_MODULES="$(CURDIR)/core/models;$(CURDIR)/pulp;$(MODULES)" \
		-DGVSOC_TARGETS="${TARGETS}" \
		-DCMAKE_SKIP_INSTALL_RPATH=false

	cd $(CURDIR) && $(CMAKE) --build build $(CMAKE_FLAGS)
	cd $(CURDIR) && $(CMAKE) --install build


clean:
	rm -rf build install

# CMAKE = /scratch2/chi/dramsys_upgrade/cmake_bin/install/bin/cmake

SYSTEMC_VERSION := 2.3.3
SYSTEMC_GIT_URL := https://github.com/accellera-official/systemc.git
SYSTEMC_INSTALL_DIR := $(PWD)/third_party/systemc_install

apply_patch:
	git submodule update --init --recursive
	cd core && git apply --check ../add_dramsyslib_patches/gvsoc_core.patch
	if [ $$? -eq 0 ]; then \
		cd core && git apply ../add_dramsyslib_patches/gvsoc_core.patch;\
	fi
	cd pulp && git apply --check ../add_dramsyslib_patches/gvsoc_pulp_modify_for_DMA_DRAM_test.patch
	if [ $$? -eq 0 ]; then \
		cd pulp && git apply ../add_dramsyslib_patches/gvsoc_pulp_modify_for_DMA_DRAM_test.patch;\
	fi


third_party:
	mkdir -p third_party

build-systemc: third_party third_party/systemc_install/lib64/libsystemc.so

third_party/systemc_install/lib64/libsystemc.so:
	mkdir -p $(SYSTEMC_INSTALL_DIR)
	cd third_party && \
	git clone $(SYSTEMC_GIT_URL) && \
	cd systemc && git fetch --tags && git checkout $(SYSTEMC_VERSION) && \
	mkdir build && cd build && \
	$(CMAKE) -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=$(SYSTEMC_INSTALL_DIR) .. && \
	make && make install

build-dramsys: build-systemc third_party/DRAMSys/libDRAMSys_Simulator.so

third_party/DRAMSys/libDRAMSys_Simulator.so: third_party
	mkdir -p third_party/DRAMSys
	cp add_dramsyslib_patches/libDRAMSys_Simulator.so third_party/DRAMSys/

build-configs: core/models/memory/dramsys_configs

core/models/memory/dramsys_configs:
	cp -rf add_dramsyslib_patches/dramsys_configs core/models/memory/

dramsys_preparation: apply_patch build-systemc build-dramsys build-configs