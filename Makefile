CMAKE_FLAGS ?= -j 6
CMAKE ?= cmake

TARGETS ?= rv32;rv64

export PATH:=$(CURDIR)/gapy/bin:$(PATH)
export PATH:=$(CURDIR)/third_party:$(PATH)

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



######################################################################
## 				Make Targets for DRAMSys Integration 				##
######################################################################

SYSTEMC_VERSION := 2.3.3
SYSTEMC_GIT_URL := https://github.com/accellera-official/systemc.git
SYSTEMC_INSTALL_DIR := $(PWD)/third_party/systemc_install

drmasys_apply_patch:
	git submodule update --init --recursive
	if cd core && git apply --check ../soft_hier/gvsoc_core.patch; then \
		git apply ../soft_hier/gvsoc_core.patch;\
	fi
	if cd pulp && git apply --check ../soft_hier/gvsoc_pulp.patch; then \
		git apply ../soft_hier/gvsoc_pulp.patch;\
	fi
	cp -rf soft_hier/flex_cluster pulp/pulp/chips/flex_cluster


build-systemc: third_party/systemc_install/lib64/libsystemc.so

third_party/systemc_install/lib64/libsystemc.so:
	mkdir -p $(SYSTEMC_INSTALL_DIR)
	cd third_party && \
	git clone $(SYSTEMC_GIT_URL) && \
	cd systemc && git fetch --tags && git checkout $(SYSTEMC_VERSION) && \
	mkdir build && cd build && \
	$(CMAKE) -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=$(SYSTEMC_INSTALL_DIR) -DCMAKE_INSTALL_LIBDIR=lib64 .. && \
	make && make install

build-dramsys: build-systemc third_party/DRAMSys/libDRAMSys_Simulator.so

third_party/DRAMSys/libDRAMSys_Simulator.so:
	mkdir -p third_party/DRAMSys
	cp soft_hier/libDRAMSys_Simulator.so third_party/DRAMSys/
	echo "Check Library Functionality"
	cd soft_hier/build_dynlib_from_github_dramsys5/dynamic_load/ && \
	gcc main.c -ldl
	@if soft_hier/build_dynlib_from_github_dramsys5/dynamic_load/a.out ; then \
        echo "Test libaray succeeded"; \
		rm soft_hier/build_dynlib_from_github_dramsys5/dynamic_load/a.out; \
		rm DRAMSysRecordable* ; \
    else \
		rm soft_hier/build_dynlib_from_github_dramsys5/dynamic_load/a.out; \
		rm third_party/DRAMSys/libDRAMSys_Simulator.so; \
		echo "Test libaray failed, We need to rebuild the library, tasks around 40 min"; \
		echo -n "Do you want to proceed? (y/n) "; \
		read -t 30 -r user_input; \
		if [ "$$user_input" = "n" ]; then echo "oops, I see, your time is precious, see you next time"; exit 1; fi; \
		echo "Go Go Go!" ; \
		cd soft_hier/build_dynlib_from_github_dramsys5 && make all; \
		cp DRAMSys/build/lib/libDRAMSys_Simulator.so ../../third_party/DRAMSys/ ; \
		make clean; \
    fi

build-configs: core/models/memory/dramsys_configs

core/models/memory/dramsys_configs:
	cp -rf soft_hier/dramsys_configs core/models/memory/

dramsys_preparation: drmasys_apply_patch build-systemc build-dramsys build-configs

sw: third_party/occamy

third_party/occamy:
	cd third_party; curl --proto '=https' --tlsv1.2 https://pulp-platform.github.io/bender/init -sSf | sh; \
	git clone https://github.com/pulp-platform/occamy.git; \
	cd occamy; git reset --hard ed0b98162fae196faff96a972f861a0aa4593227; \
	git submodule update --init --recursive; bender vendor init; \
	git apply ../../soft_hier/flex_cluster_sdk/occamy.patch; \
	cp -rfv ../../soft_hier/flex_cluster_sdk/test target/sim/sw/device/apps/blas; \
	sed -i -e '29,52d' -e '63,67d' -e '79,91d' deps/snitch_cluster/sw/snRuntime/src/start.c; \
	cd target/sim; make DEBUG=ON sw

rebuild_sw:
	cd third_party/occamy; \
	rm -rf target/sim/sw/device/apps/blas/test; \
	cp -rfv ../../soft_hier/flex_cluster_sdk/test target/sim/sw/device/apps/blas; \
	cd target/sim; make DEBUG=ON sw

clean_sw:
	rm -rf third_party/occamy

clean_dramsys_preparation:
	rm -rf third_party

update:
	rm -rf soft_hier/flex_cluster
	cp -rf pulp/pulp/chips/flex_cluster/ soft_hier/flex_cluster
	cd pulp && git diff > ../soft_hier/gvsoc_pulp.patch

config:
	rm -rf pulp/pulp/chips/flex_cluster
	cp -rf soft_hier/flex_cluster pulp/pulp/chips/flex_cluster
	python3 soft_hier/flex_cluster_utilities/config.py

iter:
	make config
	CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make TARGETS=pulp.chips.flex_cluster.flex_cluster all
	make rebuild_sw

build_softhier_hw:
	make config
	CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make TARGETS=pulp.chips.flex_cluster.flex_cluster all

run:
	./install/bin/gvsoc --target=pulp.chips.flex_cluster.flex_cluster --binary third_party/occamy/target/sim/sw/device/apps/blas/test/build/test.elf run --trace=/chip/cluster_0/redmule
