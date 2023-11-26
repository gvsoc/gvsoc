include ../utils/rt.mk

BUILDDIR = $(CURDIR)/build

all:
	mkdir -p $(BUILDDIR)
	riscv64-unknown-elf-gcc -g -o $(BUILDDIR)/test main.c $(RT_SRCS) $(RT_FLAGS)

gvsoc:
	make -C ../../../.. TARGETS=my_system MODULES=$(CURDIR) build
	g++ -o launcher launcher.cpp -I../../../../core/engine/include -L../../../../install/lib -lpulpvp

run:
	gvsoc --target-dir=$(CURDIR) --target=my_system --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test run $(runner_args)

run_launcher:
	LD_LIBRARY_PATH=$(CURDIR)/../../../../install/lib:$(LD_LIBRARY_PATH) ./launcher --config=build/gvsoc_config.json