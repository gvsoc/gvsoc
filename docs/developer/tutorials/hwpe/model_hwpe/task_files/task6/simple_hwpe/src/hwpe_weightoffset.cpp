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

int64_t Hwpe::weight_offset()
{
  this->input_layout();

  std::array<std::array<bool      , BINCONV_PER_COLUMN>, COLUMN_PER_PE> enable   ;
  std::array<std::array<WeightType, BINCONV_PER_COLUMN>, COLUMN_PER_PE> weight   ;
  std::array<OutputType,                                 COLUMN_PER_PE> sum_array;
  std::array<WeightType,                                 COLUMN_PER_PE> shift    ;
  
  std::fill(sum_array.begin(),  sum_array.end(), 0);
  std::fill(shift.begin()    ,  shift.end()    , 0);
  
  OutputType sum = 0;
  for(int i=0; i<COLUMN_PER_PE; i++) {
    if(i==0){
      enable[i].fill(true);
      weight[i].fill(1);
    }
    else{
      enable[i].fill(false);
      weight[i].fill(0);
    }
  }
  this->pe_instance_.ComputePartialSum(enable, this->input_layout_, weight, shift, sum_array, sum);
  
  for(int i=0; i<OUTPUT_LINEAR_BUFFER_SIZE; i++){
    this->output_buffer_.WriteAtIndex(i, 1, this->reg_config_.woffs_val*sum);
  }
  int64_t latency = 0; 
  ///////////////////////////////  TASK-2 //////////////////////////////
  // Assign the latency =  WEIGHT_OFFSET_LATENCY
  return latency;
}