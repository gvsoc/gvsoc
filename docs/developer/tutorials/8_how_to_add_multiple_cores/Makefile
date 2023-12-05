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

prepare:
	cp solution/* .

run:
	gvsoc --target-dir=$(CURDIR) --target=my_system --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test run $(runner_args)