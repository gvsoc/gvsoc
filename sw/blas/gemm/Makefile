# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Tim Fischer <fischeti@iis.ee.ethz.ch>

# Usage of absolute paths is required to externally include this Makefile
MK_DIR   := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
DATA_DIR := $(realpath $(MK_DIR)/data)
SRC_DIR  := $(realpath $(MK_DIR)/src)

SECTION  ?=

APP     ?= gemm
SRCS    ?= $(realpath $(SRC_DIR)/main.c)
INCDIRS ?= $(dir $(DATA_H)) $(SRC_DIR)

DATA_H    ?= $(DATA_DIR)/data.h

$(DEP): $(DATA_H)