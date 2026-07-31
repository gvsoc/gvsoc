#
# Copyright (C) 2025 ETH Zurich and University of Bologna
# SPDX-License-Identifier: Apache-2.0
# Author: Bowen Wang, ETH Zurich
#
# Spatz-specific GEMM/SpMM optimizer with L1 capacity constraint.
# Maximizes N_tile for FMA dominance while fitting double-buffered tiles in TCDM.

import copy

def opt(gemm, arch):
    """Optimize GEMM tiling for Spatz vector processor."""

    elem_size = 2 if gemm.dtype == 'fp16' else 4
    l1_capacity = arch.cluster_tcdm_size  # e.g., 0x100000 = 1MB

    m_tile = min(gemm.m_size, 64)

    # Scale: how many clusters per SUMMA group
    # Start with max N coverage, then adjust for L1
    scale_y = 1  # M is usually small, no Y split
    scale_x = min(gemm.n_size // 1, arch.num_cluster_x)  # will refine below

    # Use remaining clusters for split-K
    available_group = (arch.num_cluster_y * arch.num_cluster_x) // max(scale_x, 1)
    group_number = 1
    try_split = 2
    while try_split <= available_group:
        if gemm.k_size % try_split == 0:
            group_number = try_split
        else:
            break
        try_split *= 2

    # Determine K_tile from split-K
    if group_number > 1:
        splited_k = gemm.k_size // group_number
    else:
        splited_k = gemm.k_size

    # Now find largest N_tile and K_tile that fit in L1 (double-buffered)
    # Double-buffer: 2 × (X_tile + W_tile + Z_tile) must fit in L1
    # X_tile = M_tile × K_tile × elem_size
    # W_tile = N_tile × K_tile × elem_size  (dominant term)
    # Z_tile = M_tile × N_tile × elem_size
    # Total = 2 × (M×K + N×K + M×N) × elem_size ≤ L1

    # Strategy: fix N_tile first (maximize for FMA), then fit K_tile
    for n_tile_try in [1024, 512, 256, 128, 64]:
        if gemm.n_size % n_tile_try != 0:
            continue
        scale_x_try = min(gemm.n_size // n_tile_try, arch.num_cluster_x)
        if scale_x_try > 1 and (scale_x_try & (scale_x_try - 1)) != 0:
            scale_x_try = 1 << (scale_x_try.bit_length() - 1)
            n_tile_actual = gemm.n_size // scale_x_try
        else:
            n_tile_actual = n_tile_try

        # Find max K_tile that fits in L1
        for k_tile_try in [512, 256, 128, 64, 32]:
            if splited_k % k_tile_try != 0:
                continue
            total_l1 = 2 * (m_tile * k_tile_try + n_tile_actual * k_tile_try + m_tile * n_tile_actual) * elem_size
            if total_l1 <= l1_capacity * 0.8:  # 80% of L1 (leave room for stack etc)
                n_tile = n_tile_actual
                k_tile = k_tile_try
                scale_x = scale_x_try
                break
        else:
            continue
        break
    else:
        # Fallback: small tiles
        n_tile = 64
        k_tile = 64
        scale_x = min(gemm.n_size // n_tile, arch.num_cluster_x)

    # Recalculate available groups and split-K with final scale_x
    available_group = (arch.num_cluster_y * arch.num_cluster_x) // max(scale_x, 1)
    group_number = 1
    group_reduce = 0
    group_splitk = 0
    group_gap_x = 0
    group_gap_w = 0

    try_split = 2
    while try_split <= available_group:
        if gemm.k_size % try_split == 0:
            group_number = try_split
        else:
            break
        try_split *= 2

    if group_number > 1:
        splited_k = gemm.k_size // group_number
        # Re-check K_tile fits with split
        while k_tile > splited_k:
            k_tile //= 2
        while k_tile > 1 and splited_k % k_tile != 0:
            k_tile -= 1
        group_reduce = 1
        group_splitk = 1
        group_gap_x = splited_k * elem_size
        group_gap_w = splited_k * gemm.n_size * elem_size

    gemm.m_tile = m_tile
    gemm.n_tile = n_tile
    gemm.k_tile = k_tile
    gemm.summa_scale_x = scale_x
    gemm.summa_scale_y = scale_y
    gemm.summa_group_number = group_number
    gemm.summa_group_reduce = group_reduce
    gemm.summa_group_splitk = group_splitk
    gemm.summa_group_splitn = 0
    gemm.summa_group_gap_x = group_gap_x
    gemm.summa_group_gap_w = group_gap_w
    gemm.summa_group_gap_z = 0
    gemm.strategy = "splitK" if group_splitk else "Nosplit"

    return [gemm]
