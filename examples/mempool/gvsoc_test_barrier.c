// Copyright 2021 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// Author: Yichao Zhang, ETH Zurich

#include <stdint.h>
#include <string.h>

#include "runtime.h"
#include "synchronization.h"

volatile uint32_t result[256] __attribute__((section(".l1_prio")));

uint32_t simple_calculation(uint32_t i, uint32_t core_id) {
    return i * core_id;
}

int main() {
  uint32_t core_id = mempool_get_core_id();
  uint32_t num_cores = mempool_get_core_count();
  int32_t error = 0;
  uint32_t i = 4;

  // Initialize barrier and synchronize
  mempool_barrier_init(core_id);

//  // Test the core
//  result[core_id] = simple_calculation(i, core_id);
//
//  // Synchronize
//  mempool_barrier(num_cores);
//
//  // Result check
//  uint32_t check_id = num_cores - 1 - core_id;
//  if (result[check_id] != check_id * i) {
//      error = 1;
//  }
//
//  // Synchronize
//  mempool_barrier(num_cores);
  return error;
}

