CMAKE_FLAGS ?= -j 6
CMAKE ?= cmake

TARGETS ?= rv32;rv64
BUILDDIR ?= build
INSTALLDIR ?= install

export PATH:=$(CURDIR)/gapy-gap/bin:$(CURDIR)/gapy/bin:$(PATH)

all: checkout build

checkout:
	git submodule update --recursive --init

checkout.gap:
	@if [ ! -d "gap/.git" ]; then \
		git clone git@github.com:gvsoc/gvsoc-gap.git gap; \
	fi
	cd gap && git fetch && git checkout f07cf96a813b4939aace7fb17ce8bf0f90627c29

	@if [ ! -d "gapy-gap/.git" ]; then \
		git clone git@github.com:gvsoc/gapy.git gapy-gap; \
	fi
	cd gapy-gap && git fetch && git checkout e92ceffe2d011fc9aaa8b098fa71afbb5378d039

checkout.test:
	@if [ ! -d "tests/gvsoc-external-tests/.git" ]; then \
		git clone git@github.com:gvsoc/gvsoc-external-tests.git tests/gvsoc-external-tests; \
	fi
	cd tests/gvsoc-external-tests && git fetch && git checkout 6c7c3b58d7275963eca5eae8661298db4bf65691

.PHONY: build

build:
	@{ \
		GVSOC_MODULES="$(CURDIR)/core/models;$(CURDIR)/pulp;$(MODULES)"; \
		if [ -d "gap" ]; then \
			GVSOC_MODULES="$$GVSOC_MODULES;$(CURDIR)/gap;"; \
			OPTIONS="-DEMPTY_SFU=1"; \
		fi; \
		# Change directory to curdir to avoid issue with symbolic links \
		cd $(CURDIR) && $(CMAKE) -S . -B $(BUILDDIR) -DCMAKE_BUILD_TYPE=RelWithDebInfo \
			-DCMAKE_INSTALL_PREFIX=$(INSTALLDIR) \
			-DGVSOC_MODULES="$$GVSOC_MODULES" \
			-DGVSOC_TARGETS="${TARGETS}" \
			$${OPTIONS} \
			-DCMAKE_SKIP_INSTALL_RPATH=false; \
	}

	cd $(CURDIR) && $(CMAKE) --build $(BUILDDIR) $(CMAKE_FLAGS)
	cd $(CURDIR) && $(CMAKE) --install $(BUILDDIR)


clean:
	rm -rf $(BUILDDIR) $(INSTALLDIR)

doc:
	cd core/docs/user_manual && make html
	cd core/docs/developer_manual && make html
	@echo
	@echo "User documentation: core/docs/user_manual/_build/html/index.html"
	@echo "Developper documentation: core/docs/developer_manual/_build/html/index.html"


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
