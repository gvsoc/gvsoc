include ../utils/rt.mk

BUILDDIR = $(CURDIR)/build

clean:
	rm -f *.o $(BUILDDIR)/test
	make -C ../../../.. TARGETS=my_system MODULES=$(CURDIR) clean

all:
	mkdir -p $(BUILDDIR)
	riscv64-unknown-elf-gcc -g -o $(BUILDDIR)/test main.c $(RT_SRCS) $(RT_FLAGS)

gvsoc:
	make -C ../../../.. TARGETS=my_system MODULES=$(CURDIR) build
	g++ -o launcher launcher.cpp -I../../../../core/engine/include -L../../../../install/lib -lpulpvp

prepare:
	cp solution/* .

config:
	gvsoc --target-dir=$(CURDIR) --target=my_system --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test prepare $(runner_args)

run_launcher:
	LD_LIBRARY_PATH=$(CURDIR)/../../../../install/lib:$(LD_LIBRARY_PATH) ./launcher --config=build/gvsoc_config.json