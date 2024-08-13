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

int64_t Hwpe::output_store()
{
  int64_t total_latency = 1;
  uint8_t data[4];
  uint32_t output_val = this->output_buffer_.ReadFromIndex(this->output.iteration);
  for(int i=0; i<4; i++){
    data[i] = (output_val >> 8*i)&0xFF;
  }
  AddressType addr = CLUSTER_MASK & (this->reg_config_.output_ptr + this->output.iteration*4);
  this->io_req.init();
  this->io_req.set_addr(addr);
  this->io_req.set_size(4);
  this->io_req.set_data(data);
  this->io_req.set_is_write(1);
  int err = this->tcdm_port.req(&this->io_req);
  if (err == vp::IO_REQ_OK) {
    int64_t latency = this->io_req.get_latency();
    total_latency = total_latency > latency ? total_latency : latency;
  } else {
    this->trace.fatal("Unsupported access\n");
  }
  this->output.iteration = this->output.iteration+4;
  return total_latency;
}