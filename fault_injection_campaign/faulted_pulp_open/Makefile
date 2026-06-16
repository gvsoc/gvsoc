GVSOC_WORKDIR ?= $(CURDIR)/build
ifdef BUILDDIR
FULL_BUILDDIR ?= $(GVSOC_WORKDIR)/$(BUILDDIR)
else
FULL_BUILDDIR ?= $(GVSOC_WORKDIR)
endif

clean:
	rm -rf $(FULL_BUILDDIR)/test
	make -C ../.. TARGETS=pulp-open MODULES=$(CURDIR) BUILDDIR=$(FULL_BUILDDIR)/build INSTALLDIR=$(FULL_BUILDDIR)/install $(OPTIONS) clean

gvsoc:
	make -C ../.. TARGETS=pulp-open MODULES=$(CURDIR) BUILDDIR=$(FULL_BUILDDIR)/build INSTALLDIR=$(FULL_BUILDDIR)/install $(OPTIONS) build

test:
	$(FULL_BUILDDIR)/install/bin/gvsoc --target=pulp-open --work-dir=$(FULL_BUILDDIR)/work --binary=$(FULL_BUILDDIR)/test/test image flash run $(runner_args)

campaign:
	cd .. && ./faulted_pulp_open/complex.py
