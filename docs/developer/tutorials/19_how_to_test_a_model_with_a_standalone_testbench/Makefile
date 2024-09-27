include ../utils/rt.mk

BUILDDIR = $(CURDIR)/build

clean:
	make -C ../../../.. TARGETS=my_system MODULES=$(CURDIR) clean

all:

gvsoc:
	make -C ../../../.. TARGETS=my_system MODULES=$(CURDIR) build

prepare:
	cp solution/* .

run:
    mkdir -p $(BUILDDIR)
	gvsoc --target-dir=$(CURDIR) --target=my_system --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test run $(runner_args)