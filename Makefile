CMAKE_FLAGS ?= -j 6
CMAKE ?= cmake

TARGETS ?= rv64

export PATH:=$(CURDIR)/gapy/bin:$(PATH)

all: checkout build

checkout:
	git submodule update --recursive --init

build:
	$(CMAKE) -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DCMAKE_INSTALL_PREFIX=install \
		-DGVSOC_MODULES="$(CURDIR)/core;$(CURDIR)/pulp" \
		-DGVSOC_TARGETS="${TARGETS}"

	$(CMAKE) --build build $(CMAKE_FLAGS)
	$(CMAKE) --install build


clean:
	rm -rf build install

.PHONY: build