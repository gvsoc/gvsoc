include ../utils/rt.mk
BUILDDIR = $(CURDIR)/build

export PYTHONPATH:=$(CURDIR)/../../../../core/models:$(PYTHONPATH)

clean:
	rm -f *.o $(BUILDDIR)/test
	make -C ../../../.. TARGETS=my_system MODULES=$(CURDIR) clean

all:
	mkdir -p $(BUILDDIR)
	riscv64-unknown-elf-gcc -o $(BUILDDIR)/test main.c $(RT_SRCS) $(RT_FLAGS)

gvsoc:
	make -C ../../../.. TARGETS=my_system MODULES=$(CURDIR) build

run:
	gvsoc --target-dir=$(CURDIR) --target=my_system --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test run $(runner_args)

prepare:
	cp solution/* .

regmap:
	../../../../core/bin/regmap-gen --input-md regmap.md --header headers/mycomp