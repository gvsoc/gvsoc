/*
 * Copyright (C) 2020-2022  GreenWaves Technologies, ETH Zurich, University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * Authors: Arpan Suravi Prasad, ETH Zurich (prasadar@iis.ee.ethz.ch)
 */
#include "hwpe.hpp"

void Hwpe::weight_layout()
{
  for(int col=0; col<COLUMN_PER_PE; col++){
    for(int binconv=0; binconv<BINCONV_PER_COLUMN; binconv++){
      this->weight_temp_layout_[col][binconv] = (this->weight_temp_buf_[binconv] >> col) & 0x1;
    }
  }
}