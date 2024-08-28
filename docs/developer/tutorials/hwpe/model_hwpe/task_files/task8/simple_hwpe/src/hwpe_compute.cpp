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

int64_t Hwpe::compute_output()
{
  std::array <std::array<bool, BINCONV_PER_COLUMN>,       COLUMN_PER_PE> enable;
  std::array <std::array<WeightType, BINCONV_PER_COLUMN>, COLUMN_PER_PE> weight;
  std::array <OutputType,                                 COLUMN_PER_PE> sum_array;
  std::array <WeightType,                                 COLUMN_PER_PE> shift;

  OutputType sum = this->output_buffer_.ReadFromIndex(this->compute.iteration);

  for(int i=0; i<COLUMN_PER_PE; i++) {
      enable[i].fill(true);
      shift[i] = i;
  }
  this->pe_instance_.ComputePartialSum(enable, this->input_layout_, this->weight_temp_layout_, shift, sum_array, sum);

  this->output_buffer_.WriteAtIndex(this->compute.iteration, 1, sum);
  this->compute.iteration++;
  return 1;
}