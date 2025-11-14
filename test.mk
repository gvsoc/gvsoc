BENDER_VERSION = 0.28.1
UBUNTU_VERSION = 22.04
TIMEOUT ?= 60

GVTEST_TARGET_FLAGS = $(foreach t,$(TEST_TARGETS),--target $(t))
GVTEST_CMD = gvtest $(GVTEST_TARGET_FLAGS) --max-timeout $(TIMEOUT) run table junit summary



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
	git checkout 7c407695ac48401f0e1ebcdd4a4cc9c67e81138e

test.build.pulp-sdk: test.checkout.pulp-sdk



#
# PULP-SDK Siracusa
#

test.clean.pulp-sdk-siracusa:
	rm -rf tests/pulp-sdk-siracusa

test.checkout.pulp-sdk-siracusa:
	@if [ ! -d "tests/pulp-sdk-siracusa" ]; then \
		git clone "git@github.com:siracusa-soc/pulp-sdk.git" "tests/pulp-sdk-siracusa"; \
	fi
	cd "tests/pulp-sdk-siracusa" && \
	git fetch --all && \
	git checkout 597e0ebb12b4c7d609bdd2b09452c1f9d80031be

test.build.pulp-sdk-siracusa: test.checkout.pulp-sdk-siracusa



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


#
# Magia SDK
#

test.clean.magia:
	rm -rf tests/magia-sdk

test.checkout.magia:
	@if [ ! -d "tests/magia-sdk" ]; then \
		git clone "git@github.com:haugoug/magia-sdk.git" "tests/magia-sdk"; \
	fi
	cd "tests/magia-sdk" && \
	git fetch --all && \
	git checkout f663315d4c2ce521a5dd4ae9d08b24f2b957f100

test.build.magia: test.checkout.magia
	rm -rf $(CURDIR)/tests/magia-sdk/build
	export PATH=$(MAGIA_GCC_TOOLCHAIN)/bin:$(PATH) && cd tests/magia-sdk && $(MAKE) build compiler=GCC_PULP tiles=2 CMAKE_BUILDDIR=$(CURDIR)/tests/magia-sdk/build/tile2
	export PATH=$(MAGIA_GCC_TOOLCHAIN)/bin:$(PATH) && cd tests/magia-sdk && $(MAKE) build compiler=GCC_PULP tiles=4 CMAKE_BUILDDIR=$(CURDIR)/tests/magia-sdk/build/tile4
	export PATH=$(MAGIA_GCC_TOOLCHAIN)/bin:$(PATH) && cd tests/magia-sdk && $(MAKE) build compiler=GCC_PULP tiles=8 CMAKE_BUILDDIR=$(CURDIR)/tests/magia-sdk/build/tile8



#
# Pulp-NN
#

test.clean.pulp-nn:
	rm -rf tests/pulp-nn

test.checkout.pulp-nn:
	@if [ ! -d "tests/pulp-nn" ]; then \
		git clone "git@github.com:pulp-platform/pulp-nn-mixed.git" "tests/pulp-nn"; \
	fi
	cd "tests/pulp-nn" && \
	git fetch --all && \
	git checkout 415279ec37ef416fe43ded0e476c3fae3d17a6c8

test.build.pulp-nn: test.checkout.pulp-nn
	cd tests/pulp-nn/generators && python3 pulp_nn_examples_generator.py



#
# Mempool
#

test.clean.mempool:
	rm -rf tests/mempool

test.checkout.mempool:
	@if [ ! -d "tests/mempool" ]; then \
		git clone "git@github.com:pulp-platform/mempool.git" "tests/mempool"; \
	fi
	cd "tests/mempool" && \
	git fetch --all && \
	git checkout 84ab19ca74a421dc1c4fedbb88fd839ba9d25ffc && \
	git submodule update --recursive --init

test.build.mempool: test.checkout.mempool



test.clean: test.clean.riscv-tests test.clean.pulp-sdk test.clean.chimera-sdk test.clean.snitch \
	test.clean.spatz test.clean.ara test.clean.magia test.clean.pulp-nn test.clean.pulp-sdk-siracusa

test.checkout: test.checkout.riscv-tests test.checkout.pulp-sdk test.checkout.chimera-sdk \
	test.checkout.snitch test.checkout.spatz test.checkout.ara test.checkout.magia \
	test.checkout.pulp-nn test.checkout.pulp-sdk-siracusa

test.build: test.build.riscv-tests test.build.pulp-sdk test.build.chimera-sdk test.build.snitch \
	test.build.spatz test.build.ara test.build.magia test.build.pulp-nn test.build.pulp-sdk-siracusa

test.run:
	$(GVTEST_CMD)

test: test.build
	$(GVTEST_CMD)

.PHONY: test
