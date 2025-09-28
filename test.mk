BENDER_VERSION = 0.28.1
UBUNTU_VERSION = 22.04
TIMEOUT ?= 60

TEST_TARGETS ?= \
    pulp-open \
    chimera \
    snitch \
    snitch:core_type=fast \
    snitch_cluster_single \
    siracusa \
    rv64 \
    occamy \
    spatz \
    ara \
    snitch_spatz \
    snitch_testbench

PLPTEST_TARGET_FLAGS = $(foreach t,$(TEST_TARGETS),--target $(t))
PLPTEST_CMD = plptest $(PLPTEST_TARGET_FLAGS) --max-timeout $(TIMEOUT) run table junit



#
# Rscv tests
#

test.clean.riscv-tests:
	rm -rf tests/riscv-tests

test.checkout.riscv-tests:
	@if [ ! -d "tests/riscv-tests" ]; then \
		git clone "git@github.com:gvsoc/riscv-tests.git" "tests/riscv-tests"; \
	fi
	cd "tests/riscv-tests" && \
	git fetch --all && \
	git checkout 934ec4f4bb14f83da32cd83dcc00afa055e8717f

test.build.riscv-tests: test.checkout.riscv-tests



#
# PULP-SDK
#

test.clean.pulp-sdk:
	rm -rf tests/pulp-sdk

test.checkout.pulp-sdk:
	@if [ ! -d "tests/pulp-sdk" ]; then \
		git clone "git@github.com:pulp-platform/pulp-sdk.git" "tests/pulp-sdk"; \
	fi
	cd "tests/pulp-sdk" && \
	git fetch --all && \
	git checkout f4ff63f8bdf8a2a1e5b1e71e6604cf8ba88609b8

test.build.pulp-sdk: test.checkout.pulp-sdk



#
# CHIMERA SDK
#

test.clean.chimera-sdk:
	rm -rf tests/chimera-sdk

test.checkout.chimera-sdk:
	@if [ ! -d "tests/chimera-sdk" ]; then \
		git clone "git@github.com:pulp-platform/chimera-sdk.git" "tests/chimera-sdk"; \
	fi
	cd "tests/chimera-sdk" && \
	git fetch --all && \
	git checkout b2392f6efcff75c03f4c65eaf3e12104442b22ea

test.build.chimera-sdk: test.checkout.chimera-sdk
	cd tests/chimera-sdk && cmake -DTARGET_PLATFORM=chimera-open -DTOOLCHAIN_DIR=$(CHIMERA_LLVM) -B build
	cd tests/chimera-sdk && cmake --build build -j



#
# SNITCH RTL
#

test.clean.snitch:
	rm -rf tests/snitch

test.checkout.snitch:
	@if [ ! -d "tests/snitch" ]; then \
		git clone "git@github.com:haugoug/snitch_cluster.git" "tests/snitch"; \
	fi
	cd "tests/snitch" && \
	git fetch --all && \
	git checkout 4a2fb73b2c948958f47f7109bcb34943dcd79c14 && \
	git submodule update --recursive --init

test.build.snitch: test.checkout.snitch
	cd tests/snitch && mkdir -p install/bin && cd install/bin && wget https://github.com/pulp-platform/bender/releases/download/v$(BENDER_VERSION)/bender-$(BENDER_VERSION)-x86_64-linux-gnu-ubuntu$(UBUNTU_VERSION).tar.gz && \
		tar xzf bender-$(BENDER_VERSION)-x86_64-linux-gnu-ubuntu$(UBUNTU_VERSION).tar.gz
	cd tests/snitch && pip install .
	export PATH=$(LLVM_BINROOT):$(CURDIR)/tests/snitch/install/bin:$(PATH) && cd tests/snitch/target/snitch_cluster && $(MAKE) DEBUG=ON OPENOCD_SEMIHOSTING=ON bin/snitch_cluster.gvsoc sw


#
# SPATZ RTL
#

test.clean.spatz:
	rm -rf tests/spatz-rtl

test.checkout.spatz:
	@if [ ! -d "tests/spatz-rtl" ]; then \
		git clone "git@github.com:haugoug/spatz.git" "tests/spatz-rtl"; \
	fi
	cd "tests/spatz-rtl" && \
	git fetch --all && \
	git checkout 04a9859089d5f25a732a31f3c9eb8fa9e19a1621

test.build.spatz: test.checkout.spatz
	cd tests/spatz-rtl && mkdir -p install/bin && cd install/bin && wget https://github.com/pulp-platform/bender/releases/download/v$(BENDER_VERSION)/bender-$(BENDER_VERSION)-x86_64-linux-gnu-ubuntu$(UBUNTU_VERSION).tar.gz && \
		tar xzf bender-$(BENDER_VERSION)-x86_64-linux-gnu-ubuntu$(UBUNTU_VERSION).tar.gz
	cd tests/spatz-rtl && $(MAKE) sw/toolchain/riscv-opcodes BENDER=$(CURDIR)/tests/spatz-rtl/install/bin/bender
	unset CMAKE_GENERATOR && export PATH=$(LLVM_BINROOT):$(CURDIR)/tests/snitch/install/bin:$(PATH) && cd tests/spatz-rtl/hw/system/spatz_cluster && $(MAKE) sw.vsim GCC_INSTALL_DIR=$(SPATZ_GCC) VSIM_HOME=$(VSIM_HOME) BENDER=$(CURDIR)/tests/spatz-rtl/install/bin/bender CMAKE=cmake VSIM=vsim VLOG=vlog LLVM_INSTALL_DIR=$(SPATZ_LLVM) -j1


#
# ARA RTL
#

test.clean.ara:
	rm -rf tests/ara-rtl

test.checkout.ara:
	@if [ ! -d "tests/ara-rtl" ]; then \
		git clone "git@github.com:pulp-platform/ara.git" "tests/ara-rtl"; \
	fi
	cd "tests/ara-rtl" && \
	git fetch --all && \
	git checkout 05c1616d2317d2fda09b3fdf69e8aae4c2e55eaf

test.build.ara: test.checkout.ara
	cd tests/ara-rtl/apps && make riscv_tests GCC_INSTALL_DIR=$(RISCV_GCC) LLVM_INSTALL_DIR=$(ARA_LLVM)




test.clean: test.clean.riscv-tests test.clean.pulp-sdk test.clean.chimera-sdk test.clean.snitch test.clean.spatz test.clean.ara

test.checkout: test.checkout.riscv-tests test.checkout.pulp-sdk test.checkout.chimera-sdk test.checkout.snitch test.checkout.spatz test.checkout.ara

test.build: test.build.riscv-tests test.build.pulp-sdk test.build.chimera-sdk test.build.snitch test.build.spatz test.build.ara

test.run:
	$(PLPTEST_CMD)

test: test.build
	$(PLPTEST_CMD)

.PHONY: test
