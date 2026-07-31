#
# Copyright (C) 2025 ETH Zurich and University of Bologna
# SPDX-License-Identifier: Apache-2.0
# Author: Bowen Wang, ETH Zurich

class SpatzSpMM:
    """
    Spatz SpMM config for SUMMA dataflow with N:M sparsity.
    Compatible with gemm_auto optimizer for SUMMA tiling.
    """

    def __init__(self):

        self.dtype                   = 'fp16'
        self.m_size                  = 64
        self.n_size                  = 64
        self.k_size                  = 64

        # Sparsity
        self.n_sparse                = 2
        self.m_sparse                = 4

        # Tile (set by optimizer)
        self.m_tile                  = 64
        self.n_tile                  = 64
        self.k_tile                  = 64

        # SUMMA
        self.summa_scale_x           = 4
        self.summa_scale_y           = 4
        self.summa_group_number      = 1
        self.summa_group_reduce      = 0
        self.summa_group_splitk      = 0
        self.summa_group_splitn      = 0
        self.summa_group_gap_x       = 0
        self.summa_group_gap_w       = 0
        self.summa_group_gap_z       = 0

        # Reshape
        self.resha_x_from_enable     = 0
        self.resha_z_to_enable       = 0
        self.resha_x_from_m          = 64
        self.resha_z_to_m            = 64

        self.summa_numer             = 0
        self.summa_numer_chunk       = 8192
