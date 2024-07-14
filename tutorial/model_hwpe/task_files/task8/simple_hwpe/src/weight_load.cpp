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

int64_t Hwpe::weight_load()
{
  int64_t max_latency = 0;
  uint8_t data[8];
  for(int i=0; i<2; i++)
  {
    AddressType addr = CLUSTER_MASK & (this->reg_config_.weight_ptr + this->weight.iteration );
    this->io_req.init();
    this->io_req.set_addr(addr);
    this->io_req.set_size(4);
    this->io_req.set_data(data+4*i);
    this->io_req.set_is_write(0);
    int err = this->tcdm_port.req(&this->io_req);
    if (err == vp::IO_REQ_OK) {
      int64_t latency = this->io_req.get_latency();
      max_latency = max_latency > latency ? max_latency : latency;
#ifdef VERBOSE
    uint32_t data_word = ((*(data+i*4)) & 0xFF) + (((*(data+i*4+1)) & 0xFF)<<8) + (((*(data+i*4+2)) & 0xFF)<<16) + (((*(data+i*4+3)) & 0xFF)<<24);
    this->trace.msg("weight load max_latency=%d, latency=%d, addr=0x%x, data=0x%x\n", max_latency, latency, (addr), data_word);
#endif
    } else {
      this->trace.fatal("Unsupported access\n");
    }
    for(int j=0; j<4; j++)
    {
      this->weight_temp_buf_[this->weight.iteration] = data[i*4+j];
      this->weight.iteration++;
    }
  }
  return max_latency+1;
}
