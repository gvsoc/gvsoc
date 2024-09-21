CMAKE_FLAGS ?= -j 6
CMAKE ?= cmake

TARGETS ?= rv32;rv64
BUILDDIR ?= build
INSTALLDIR ?= install

export PATH:=$(CURDIR)/gapy/bin:$(PATH)

all: checkout build

checkout:
	git submodule update --recursive --init

.PHONY: build

build:
	# Change directory to curdir to avoid issue with symbolic links
	cd $(CURDIR) && $(CMAKE) -S . -B $(BUILDDIR) -DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DCMAKE_INSTALL_PREFIX=$(INSTALLDIR) \
		-DGVSOC_MODULES="$(CURDIR)/core/models;$(CURDIR)/pulp;$(MODULES)" \
		-DGVSOC_TARGETS="${TARGETS}" \
		-DCMAKE_SKIP_INSTALL_RPATH=false

	cd $(CURDIR) && $(CMAKE) --build $(BUILDDIR) $(CMAKE_FLAGS)
	cd $(CURDIR) && $(CMAKE) --install $(BUILDDIR)


clean:
	rm -rf $(BUILDDIR) $(INSTALLDIR)



######################################################################
## 				Make Targets for DRAMSys Integration 				##
######################################################################

SYSTEMC_VERSION := 2.3.3
SYSTEMC_GIT_URL := https://github.com/accellera-official/systemc.git
SYSTEMC_INSTALL_DIR := $(PWD)/third_party/systemc_install

drmasys_apply_patch:
	git submodule update --init --recursive
	if cd core && git apply --check ../add_dramsyslib_patches/gvsoc_core.patch; then \
		git apply ../add_dramsyslib_patches/gvsoc_core.patch;\
	fi
	if cd pulp && git apply --check ../add_dramsyslib_patches/gvsoc_pulp.patch; then \
		git apply ../add_dramsyslib_patches/gvsoc_pulp.patch;\
	fi


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
	cp add_dramsyslib_patches/libDRAMSys_Simulator.so third_party/DRAMSys/
	echo "Check Library Functionality"
	cd add_dramsyslib_patches/build_dynlib_from_github_dramsys5/dynamic_load/ && \
	gcc main.c -ldl
	@if add_dramsyslib_patches/build_dynlib_from_github_dramsys5/dynamic_load/a.out ; then \
        echo "Test libaray succeeded"; \
		rm add_dramsyslib_patches/build_dynlib_from_github_dramsys5/dynamic_load/a.out; \
		rm DRAMSysRecordable* ; \
    else \
		rm add_dramsyslib_patches/build_dynlib_from_github_dramsys5/dynamic_load/a.out; \
		rm third_party/DRAMSys/libDRAMSys_Simulator.so; \
		echo "Test libaray failed, We need to rebuild the library, tasks around 40 min"; \
		echo -n "Do you want to proceed? (y/n) "; \
		read -t 30 -r user_input; \
		if [ "$$user_input" = "n" ]; then echo "oops, I see, your time is precious, see you next time"; exit 1; fi; \
		echo "Go Go Go!" ; \
		cd add_dramsyslib_patches/build_dynlib_from_github_dramsys5 && make all; \
		cp DRAMSys/build/lib/libDRAMSys_Simulator.so ../../third_party/DRAMSys/ ; \
		make clean; \
    fi

build-configs: core/models/memory/dramsys_configs

core/models/memory/dramsys_configs:
	cp -rf add_dramsyslib_patches/dramsys_configs core/models/memory/

dramsys_preparation: drmasys_apply_patch build-systemc build-dramsys build-configs

clean_dramsys_preparation:
	rm -rf third_party

######################################################################
## 				Snitch cluster testsuite			 				##
######################################################################

snitch_cluster.checkout:
	git clone git@github.com:pulp-platform/snitch_cluster.git -b gvsoc-ci
	cd snitch_cluster && git submodule update --recursive --init

snitch_cluster.build:
	cd snitch_cluster/target/snitch_cluster && make DEBUG=ON OPENOCD_SEMIHOSTING=ON sw

snitch_cluster.test:
	cd snitch_cluster/target/snitch_cluster && GVSOC_TARGET=$(TARGETS) ./util/run.py sw/run.yaml --simulator gvsoc -j

snitch_cluster: snitch_cluster.checkout snitch_cluster.build snitch_cluster.test