include test.mk

CMAKE_FLAGS ?= -j 16
CMAKE ?= cmake

ifeq ($(TARGETS),)

TARGETS ?= rv64 \
    rv64_untimed \
    pulp-open \
    pulp-open-nn \
    pulp-open:chip/cluster/redmule=True \
    pulp.spatz.spatz \
    snitch_spatz \
    occamy \
    siracusa \
    snitch \
    ara \
    ara_v2 \
    spatz \
    spatz_v2 \
    snitch:core_type=fast \
    pulp.snitch.snitch_cluster_single \
    chimera \
    snitch_testbench \
    magia_v2 \
	mempool

RUN_TARGETS = $(TARGETS) default

else

RUN_TARGETS = $(TARGETS)

endif

GVTEST_TARGET_FLAGS = $(foreach t,$(subst ;, ,$(RUN_TARGETS)),--target $(t))

ifndef BUILDDIR
ifdef GVSOC_WORKDIR
BUILDDIR = $(GVSOC_WORKDIR)/build
else
BUILDDIR = build
endif
endif

ifndef INSTALLDIR
ifdef GVSOC_WORKDIR
INSTALLDIR = $(GVSOC_WORKDIR)/install
else
INSTALLDIR = install
endif
endif

# Absolute install path, needed for things like configure --prefix and rpaths
INSTALLDIR_ABS := $(abspath $(INSTALLDIR))

export PATH:=$(CURDIR)/gapy/bin:$(PATH)

all: checkout build

checkout:
	git submodule update --recursive --init

.PHONY: build gui deps

ifdef DEBUG
BUILD_TYPE = RelWithDebInfo
else
BUILD_TYPE = Release
endif

gvrun.build:
	$(CMAKE) -S gvrun -B $(BUILDDIR)/gvrun -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALLDIR)

	cmake --build $(BUILDDIR)/gvrun $(CMAKE_FLAGS)
	cmake --install $(BUILDDIR)/gvrun

	$(CMAKE) -S config_tree -B $(BUILDDIR)/config_tree -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALLDIR)

	cmake --build $(BUILDDIR)/config_tree $(CMAKE_FLAGS)
	cmake --install $(BUILDDIR)/config_tree

build: gvrun.build
	# Change directory to curdir to avoid issue with symbolic links
	# Bake install/lib into the build RPATH. The generated model .so files are
	# installed as plain files (no install-time RPATH rewrite), so they keep
	# their build RPATH; adding install/lib there lets a privately-built libdw
	# (see the `deps` target) be found at run time without LD_LIBRARY_PATH.
	cd $(CURDIR) && $(CMAKE) -S . -B $(BUILDDIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALLDIR) \
		-DCMAKE_BUILD_RPATH=$(INSTALLDIR_ABS)/lib \
		-DGVSOC_MODULES="$(CURDIR)/engine/python;$(CURDIR)/core/models;$(CURDIR)/pulp;$(CURDIR)/pulp/targets;$(CURDIR)/gvrun/python;$(CURDIR)/config_tree;$(MODULES)" \
		-DGVSOC_TARGETS="${TARGETS}" \
		-DCMAKE_SKIP_INSTALL_RPATH=false

	# CPATH/LIBRARY_PATH let the compiler find the privately-built libdw at
	# build time. The dirs are searched after the regular ones, so this is a
	# no-op when they don't exist or when the system already provides libdw.
	cd $(CURDIR) && \
		CPATH="$(INSTALLDIR_ABS)/include:$$CPATH" \
		LIBRARY_PATH="$(INSTALLDIR_ABS)/lib:$$LIBRARY_PATH" \
		$(CMAKE) --build $(BUILDDIR) $(CMAKE_FLAGS)
	cd $(CURDIR) && $(CMAKE) --install $(BUILDDIR)


clean:
	rm -rf $(BUILDDIR) $(INSTALLDIR)


######################################################################
## 				Third-party dependencies						 				##
######################################################################
#
# libdw (provided by elfutils) is needed by gvsoc at build/link and run
# time. It is installed on most machines, so this is NOT wired into the
# build. On the (few) hosts that lack it, run `make deps` once: it builds
# libdw (and the libelf it depends on) from source and installs them into
# the SDK install folder. The build passes -DCMAKE_PREFIX_PATH=$(INSTALLDIR),
# so once present this private copy is picked up automatically; otherwise
# the build falls back to the system libdw.

ELFUTILS_VERSION := 0.191
ELFUTILS_URL := https://sourceware.org/elfutils/ftp/$(ELFUTILS_VERSION)/elfutils-$(ELFUTILS_VERSION).tar.bz2
ELFUTILS_SRC := $(CURDIR)/third_party/elfutils-$(ELFUTILS_VERSION)

# libdw.so is installed by elfutils; use it as the build stamp so deps is
# only run once.
deps: $(INSTALLDIR_ABS)/lib/libdw.so

$(INSTALLDIR_ABS)/lib/libdw.so:
	mkdir -p $(INSTALLDIR_ABS) third_party
	# Download if missing (ignore wget's exit code; some wget builds return
	# non-zero even on a complete download) and let tar be the integrity gate.
	cd third_party && \
		if [ ! -f elfutils-$(ELFUTILS_VERSION).tar.bz2 ]; then \
			wget -q $(ELFUTILS_URL) || true; \
		fi && \
		if [ ! -d elfutils-$(ELFUTILS_VERSION) ]; then \
			tar xf elfutils-$(ELFUTILS_VERSION).tar.bz2; \
		fi
	# -Wno-error keeps newer compilers from failing elfutils' -Werror build;
	# debuginfod is disabled to avoid pulling in libcurl.
	cd $(ELFUTILS_SRC) && \
		./configure --prefix=$(INSTALLDIR_ABS) \
			--disable-debuginfod --disable-libdebuginfod \
			CFLAGS="-g -O2 -Wno-error" CXXFLAGS="-g -O2 -Wno-error"
	# Build the whole tree so the static backends that libdw.so pulls in
	# (libebl, libcpu, libdwelf, libdwfl, ...) are produced in the right
	# order, then install only the libs/headers gvsoc needs (libelf, libdw
	# and the libdwfl header used by the iss trace model) to keep the folder
	# clean.
	$(MAKE) -C $(ELFUTILS_SRC)
	$(MAKE) -C $(ELFUTILS_SRC)/libelf install
	$(MAKE) -C $(ELFUTILS_SRC)/libdw install
	$(MAKE) -C $(ELFUTILS_SRC)/libdwfl install

github.test:
	gvtest $(GVTEST_TARGET_FLAGS) --testset testset-github.cfg --max-timeout 120 --no-fail run table junit

riscv:
	wget https://github.com/riscv-collab/riscv-gnu-toolchain/releases/download/2025.01.17/riscv64-elf-ubuntu-22.04-gcc-nightly-2025.01.17-nightly.tar.xz
	tar xvf riscv64-elf-ubuntu-22.04-gcc-nightly-2025.01.17-nightly.tar.xz

test.withbuild: riscv
	PATH=$(CURDIR)/riscv/bin:$(PATH) && gvtest --testset testset_withbuild.cfg  --thread 1 --no-fail run table junit

doc:
	cd core/docs/user_manual && make html
	cd core/docs/developer_manual && make html
	@echo
	@echo "User documentation: core/docs/user_manual/_build/html/index.html"
	@echo "Developper documentation: core/docs/developer_manual/_build/html/index.html"


######################################################################
## 				Make Targets for DRAMSys Integration 				##
######################################################################

SYSTEMC_VERSION := 3.0.1
SYSTEMC_GIT_URL := https://github.com/accellera-official/systemc.git
SYSTEMC_INSTALL_DIR := $(PWD)/third_party/systemc_install

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
        echo "Test library succeeded"; \
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

dramsys_preparation: build-systemc build-dramsys build-configs

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


gui:
	@if [ ! -d "gui-release" ]; then \
		git clone "git@github.com:gvsoc/gui.git" "gui-release"; \
	fi
	cd "gui-release" && \
	git fetch --all && \
	git checkout 30f4f9f2e149714dd0dabd3b7b65c8b9ba60c600
	mkdir -p $(INSTALLDIR)
	cp -r gui-release/* $(INSTALLDIR)
