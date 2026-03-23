#
# Copyright (C) 2025 ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Author: Bowen Wang, ETH Zurich

#      K         N            N
#   |-----|   |-----|      |-----|
# M |  X  | x |  W  | K => |  Z  | M
#   |-----|   |-----|      |-----|

class SpatzGEMM:
    """
    Spatz GEMM configuration for SUMMA dataflow.
    Same structure as SummaGEMM to be compatible with gemm_auto optimizer.
    """

    def __init__(self):

        # GEMM parameters
        self.dtype                   = 'fp32'
        self.m_size                  = 64
        self.n_size                  = 64
        self.k_size                  = 64

        # Tile configuration (set by optimizer)
        self.m_tile                  = 64
        self.n_tile                  = 64
        self.k_tile                  = 64

        # SUMMA scale (cluster groups)
        self.summa_scale_x           = 4
        self.summa_scale_y           = 4

        # Group configuration
        self.summa_group_number      = 1
        self.summa_group_reduce      = 0
        self.summa_group_splitk      = 0
        self.summa_group_splitn      = 0
        self.summa_group_gap_x       = 0
        self.summa_group_gap_w       = 0
        self.summa_group_gap_z       = 0

        # Reshape options (needed for optimizer compatibility)
        self.resha_x_from_enable     = 0
        self.resha_z_to_enable       = 0
        self.resha_x_from_m          = 64
        self.resha_z_to_m            = 64

        # Numerical validation
        self.summa_numer             = 0
        self.summa_numer_chunk       = 8192
