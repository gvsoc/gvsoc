# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Luca Colagrande <colluca@iis.ee.ethz.ch>

# Usage of absolute paths is required to externally include
# this Makefile from multiple different locations
MK_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
include $(MK_DIR)/./toolchain.mk

###############
# Directories #
###############

# Fixed paths in repository tree
ROOT     := $(abspath $(MK_DIR)/../..)
SNRT_DIR := $(ROOT)/sw/snRuntime
RUNTIME_DIR := $(ROOT)/sw/runtime/rtl
MATH_DIR := $(ROOT)/sw/math

# Paths relative to the app including this Makefile
APP_BUILDDIR ?= $(abspath build)

###################
# Build variables #
###################

INCDIRS += $(RUNTIME_DIR)/src
INCDIRS += $(RUNTIME_DIR)/../common
INCDIRS += $(SNRT_DIR)/api
INCDIRS += $(SNRT_DIR)/api/omp
INCDIRS += $(SNRT_DIR)/src
INCDIRS += $(SNRT_DIR)/src/omp
INCDIRS += $(MATH_DIR)/include
INCDIRS += $(MATH_DIR)/arch/riscv64/
INCDIRS += $(MATH_DIR)/arch/generic
INCDIRS += $(MATH_DIR)/src/include
INCDIRS += $(MATH_DIR)/src/math
INCDIRS += $(MATH_DIR)/src/internal
INCDIRS += $(MATH_DIR)/include/bits
INCDIRS += $(ROOT)/sw/blas
# INCDIRS += $(ROOT)/sw/blas/axpy/data
INCDIRS += $(ROOT)/sw/blas/gemm/data
INCDIRS += $(ROOT)/sw/deps/riscv-opcodes

LIBS  = $(MATH_DIR)/libmath.a
LIBS += $(RUNTIME_DIR)/libsnRuntime.a

LIBDIRS  = $(dir $(LIBS))
LIBNAMES = $(patsubst lib%,%,$(notdir $(basename $(LIBS))))

RISCV_LDFLAGS += -L$(abspath $(RUNTIME_DIR))
RISCV_LDFLAGS += -T$(abspath $(SNRT_DIR)/base.ld)
RISCV_LDFLAGS += $(addprefix -L,$(LIBDIRS))
RISCV_LDFLAGS += $(addprefix -l,$(LIBNAMES))

SRCS += $(RUNTIME_DIR)/src/snitch_cluster_start.S
SRCS += $(RUNTIME_DIR)/src/snrt.c

###########
# Outputs #
###########

ELF         = $(abspath $(addprefix $(APP_BUILDDIR)/,$(addsuffix .elf,$(APP))))
DEP         = $(abspath $(addprefix $(APP_BUILDDIR)/,$(addsuffix .d,$(APP))))

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP)
endif