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
#define VERBOSE

//////////////////////////   SOLUTION-1 //////////////////////////////////
// Uncomment the following 
#define EFFICIENT_IMPLEMNTATION 

#ifndef EFFICIENT_IMPLEMNTATION 
int64_t Hwpe::input_load()
{
  int64_t max_latency = 0;
  uint8_t data[8];
  for(int i=0; i<8; i++)
  {
    AddressType addr = CLUSTER_MASK & (this->reg_config_.input_ptr + this->input.iteration);
    this->io_req.init();
    this->io_req.set_addr(addr);
    this->io_req.set_size(1);
    this->io_req.set_data(&data[i]);
    this->io_req.set_is_write(0);
    this->io_req.set_debug(true);
    int err = this->tcdm_port.req(&this->io_req);
    if (err == vp::IO_REQ_OK) {
      int64_t latency = this->io_req.get_latency();
      max_latency = max_latency > latency ? max_latency : latency;
#ifdef VERBOSE
      this->trace.msg("input load max_latency=%d, latency=%d, addr=0x%x, data=0x%x\n", max_latency, latency, (addr), data[i]);
#endif
    } else {
      this->trace.fatal("Unsupported access\n");
    }
    this->trace.msg("Load done for addr=%d, data=%d\n", (addr), data[i]);
    this->input_buffer_.WriteAtIndex(this->input.iteration, 1, data[i]);
    this->input.iteration++;
  }
  return max_latency+1;
}
#else 
int64_t Hwpe::input_load()
{
  int64_t max_latency = 0;
  uint8_t data[8];
  for(int i=0; i<2; i++)
  {
    AddressType addr = CLUSTER_MASK & (this->reg_config_.input_ptr + this->input.iteration);
    this->io_req.init();
    this->io_req.set_addr(addr);
    this->io_req.set_size(4);
    this->io_req.set_data(data+i*4);
    this->io_req.set_is_write(0);
    this->io_req.set_debug(true);

    int err = this->tcdm_port.req(&this->io_req);
    if (err == vp::IO_REQ_OK) {
      int64_t latency = this->io_req.get_latency();
      max_latency = max_latency > latency ? max_latency : latency;
#ifdef VERBOSE
      uint32_t data_word = ((*(data+i*4)) & 0xFF) + (((*(data+i*4+1)) & 0xFF)<<8) + (((*(data+i*4+2)) & 0xFF)<<16) + (((*(data+i*4+3)) & 0xFF)<<24);
      this->trace.msg("input load max_latency=%d, latency=%d, addr=%d, data=0x%x\n", max_latency, latency, (addr), data_word);
#endif
    } else {
      this->trace.fatal("Unsupported access\n");
    }
    for(int j=0; j<4; j++)
    {
      this->input_buffer_.WriteAtIndex(this->input.iteration+j, 1, data[i*4+j]);
    }
    this->input.iteration = this->input.iteration+4;
  }
  return max_latency+1;
}
#endif