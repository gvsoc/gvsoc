include ../snRuntime/common.mk
include ../blas/gemm/Makefile

BUILDDIR = $(CURDIR)/build

clean:
	rm -f *.o $(BUILDDIR)/test
	CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make -C ../../ TARGETS=pulp.snitch.snitch_cluster_single MODULES=$(CURDIR) clean

sw:
	mkdir -p $(BUILDDIR)
	# gemm test
	$(RISCV_CC) -g -o $(BUILDDIR)/test ../blas/gemm/src/main.c $(SRCS) $(RISCV_CFLAGS) $(RISCV_LDFLAGS)
	# axpy test
	# $(RISCV_CC) -g -o $(BUILDDIR)/test main.c $(SRCS) $(RISCV_CFLAGS) $(RISCV_LDFLAGS)

gvsoc:
	CXX=g++-11.2.0 CC=gcc-11.2.0 CMAKE=cmake-3.18.1 make -C ../../ TARGETS=pulp.snitch.snitch_cluster_single MODULES=$(CURDIR) build

run:
	../../install/bin/gvsoc --target-dir=$(CURDIR) --target=pulp.snitch.snitch_cluster_single --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test image flash run 

all: clean sw gvsoc run