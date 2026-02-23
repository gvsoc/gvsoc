CMAKE_VERSION := 3.28.1
CMAKE_DIR := cmake-$(CMAKE_VERSION)
CMAKE_TARBALL := $(CMAKE_DIR).tar.gz
CMAKE_DOWNLOAD_URL := https://cmake.org/files/v3.28/$(CMAKE_TARBALL)
CMAKE_BUILD_DIR := ./cmake_bin

$(CMAKE_TARBALL):
	wget $(CMAKE_DOWNLOAD_URL)

$(CMAKE_DIR): $(CMAKE_TARBALL)
	tar -xzvf $(CMAKE_TARBALL)

$(PWD)/$(CMAKE_BUILD_DIR)/bin/cmake: $(CMAKE_DIR)
	mkdir -p $(CMAKE_BUILD_DIR)
	cd $(CMAKE_DIR) && \
	./bootstrap --prefix=$(PWD)/$(CMAKE_BUILD_DIR) && \
	make && \
	make install

CMAKE = $(PWD)/$(CMAKE_BUILD_DIR)/bin/cmake
# CMAKE = /scratch2/chi/dramsys_upgrade/cmake_bin/install/bin/cmake

DRAMSys/src/simulator/simulator/dramsys_lib.cpp:
	git clone https://github.com/tukl-msd/DRAMSys.git && \
	cd DRAMSys && \
	git reset --hard 8e021ea && \
	git apply ../patch
	echo "DRAMSys setup done"

DRAMSys/build/lib/libDRAMSys_Simulator.so: $(CMAKE) DRAMSys/src/simulator/simulator/dramsys_lib.cpp
	cd DRAMSys && mkdir build
	cd DRAMSys/build && \
	$(CMAKE) \
	-DCMAKE_CXX_FLAGS=-fPIC \
	-DCMAKE_C_FLAGS=-fPIC \
	-D DRAMSYS_WITH_DRAMPOWER=ON .. && \
	make

build: DRAMSys/build/lib/libDRAMSys_Simulator.so

all: build

clean:
	if [ -d "DRAMSys" ]; then rm -rf DRAMSys; fi
	if [ -d "$(CMAKE_BUILD_DIR)" ]; then rm -rf $(CMAKE_BUILD_DIR); fi
	if [ -d "$(CMAKE_DIR)" ]; then rm -rf $(CMAKE_DIR); fi
	if [ -f "$(CMAKE_TARBALL)" ]; then rm $(CMAKE_TARBALL); fi
