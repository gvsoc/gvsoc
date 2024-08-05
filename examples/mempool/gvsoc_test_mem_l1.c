// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Yichao Zhang, ETH Zurich

#include <stdint.h>
#include <string.h>

#include "encoding.h"
#include "printf.h"
#include "runtime.h"
#include "synchronization.h"

int main() {
  uint32_t core_id = mempool_get_core_id();
  uint32_t num_cores = mempool_get_core_count();

  // Initialize synchronization variables
  mempool_barrier_init(core_id);

 // if (core_id < 1) {
 //   volatile int nb_loop = 3;
 //   for (int i=0; i<nb_loop; i++) {
 //     __asm__ __volatile__ (
 //     "li t0, 0x80000000;"
 //     "lw a0, 0(t0);"
 //     "lw a1, 4(t0);"
 //     "lw a2, 8(t0);"
 //     "lw a3, 12(t0);"
 //     "lw a4, 16(t0);"
 //     "lw a5, 20(t0);"
 //     "lw a6, 24(t0);"
 //     "lw a7, 28(t0);"
 //     "add t1, a0, a1;"
 //     "add t2, a2, a3;"
 //     "add t3, a4, a5;"
 //     "add t4, a6, a7;"
 //     "sw t1, 100(t0);"
 //     "sw t2, 104(t0);"
 //     "sw t3, 108(t0);"
 //     "sw t4, 112(t0);"
 //     "sw x13, 116(t0);"
 //   :
 //   :
 //   : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3", "t4"
 //   );
 //   }
 // }

  // wait until all cores have finished
   mempool_barrier(num_cores);
  return 0;
}

