include test.mk

CMAKE_FLAGS ?= -j 16
CMAKE ?= cmake

ifeq ($(TARGETS),)

TARGETS ?= rv64 \
    rv64_untimed \
    pulp-open \
    pulp-open-nn \
    pulp-open:attr.chip/cluster/has_redmule=true \
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
	# their build RPATH; adding install/lib there lets a privately-built
	# libdwarf (see the `deps` target) be found at run time without
	# LD_LIBRARY_PATH.
	#
	# PKG_CONFIG_PATH lets cmake's pkg_check_modules(libdwarf) discover a
	# privately-built libdwarf at configure time (its headers live in a
	# versioned subdir, so the include path must come from pkg-config). The dir
	# is searched after the regular ones, so this is a no-op when it does not
	# exist or when the system already provides libdwarf.
	cd $(CURDIR) && \
		PKG_CONFIG_PATH="$(INSTALLDIR_ABS)/lib/pkgconfig:$$PKG_CONFIG_PATH" \
		$(CMAKE) -S . -B $(BUILDDIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALLDIR) \
		-DCMAKE_BUILD_RPATH=$(INSTALLDIR_ABS)/lib \
		-DGVSOC_MODULES="$(CURDIR)/engine/python;$(CURDIR)/core/models;$(CURDIR)/pulp;$(CURDIR)/pulp/targets;$(CURDIR)/gvrun/python;$(CURDIR)/config_tree;$(MODULES)" \
		-DGVSOC_TARGETS="${TARGETS}" \
		-DCMAKE_SKIP_INSTALL_RPATH=false

	# PKG_CONFIG_PATH/LIBRARY_PATH let cmake and the linker find the
	# privately-built libdwarf at build time. Searched after the regular dirs,
	# so a no-op when absent or when the system already provides libdwarf.
	cd $(CURDIR) && \
		PKG_CONFIG_PATH="$(INSTALLDIR_ABS)/lib/pkgconfig:$$PKG_CONFIG_PATH" \
		LIBRARY_PATH="$(INSTALLDIR_ABS)/lib:$$LIBRARY_PATH" \
		$(CMAKE) --build $(BUILDDIR) $(CMAKE_FLAGS)
	cd $(CURDIR) && $(CMAKE) --install $(BUILDDIR)


clean:
	rm -rf $(BUILDDIR) $(INSTALLDIR)


######################################################################
## 				Third-party dependencies						 				##
######################################################################
#
# libdwarf is needed by gvsoc at build/link and run time (the ISS and GUI
# resolve trace symbols through it). It is installed on most machines, so this
# is NOT wired into the build. On the (few) hosts that lack it, run `make deps`
# once: it builds libdwarf from source and installs it into the SDK install
# folder. The build adds that folder to PKG_CONFIG_PATH/LIBRARY_PATH (see the
# `build` target), so once present this private copy is picked up automatically;
# otherwise the build falls back to the system libdwarf.

LIBDWARF_VERSION := 0.11.1
LIBDWARF_URL := https://github.com/davea42/libdwarf-code/releases/download/v$(LIBDWARF_VERSION)/libdwarf-$(LIBDWARF_VERSION).tar.xz
LIBDWARF_SRC := $(CURDIR)/third_party/libdwarf-$(LIBDWARF_VERSION)

# libdwarf.so is installed by the build; use it as the stamp so deps runs once.
deps: $(INSTALLDIR_ABS)/lib/libdwarf.so

$(INSTALLDIR_ABS)/lib/libdwarf.so:
	mkdir -p $(INSTALLDIR_ABS) third_party
	# Download if missing (ignore wget's exit code; some wget builds return
	# non-zero even on a complete download) and let tar be the integrity gate.
	cd third_party && \
		if [ ! -f libdwarf-$(LIBDWARF_VERSION).tar.xz ]; then \
			wget -q $(LIBDWARF_URL) || true; \
		fi && \
		if [ ! -d libdwarf-$(LIBDWARF_VERSION) ]; then \
			tar xf libdwarf-$(LIBDWARF_VERSION).tar.xz; \
		fi
	# Shared lib only. `make install` installs libdwarf.so, the versioned
	# headers (include/libdwarf-0/) and a libdwarf.pc that the build locates
	# via PKG_CONFIG_PATH. zlib/zstd are optional (only used for compressed
	# DWARF, which these bare-metal binaries do not have).
	cd $(LIBDWARF_SRC) && \
		./configure --prefix=$(INSTALLDIR_ABS) --enable-shared --disable-static \
			CFLAGS="-g -O2" CXXFLAGS="-g -O2"
	$(MAKE) -C $(LIBDWARF_SRC)
	$(MAKE) -C $(LIBDWARF_SRC) install

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
	git checkout 4800fc0c6c2b75e8dbc8c0c60670310ae606cb51
	mkdir -p $(INSTALLDIR)
	cp -r gui-release/* $(INSTALLDIR)
