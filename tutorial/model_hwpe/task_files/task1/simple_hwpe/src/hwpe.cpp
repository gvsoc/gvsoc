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
 * Authors: Francesco Conti, University of Bologna & GreenWaves Technologies (f.conti@unibo.it)
 *          Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 *          Arpan Suravi Prasad, ETH Zurich (prasadar@iis.ee.ethz.ch)
 */

#include "hwpe.hpp"

Hwpe::Hwpe(vp::ComponentConf &config)
    : vp::Component(config)
{
  this->traces.new_trace("trace", &this->trace, vp::DEBUG);
  this->new_slave_port("config", &this->cfg_port_); 
  this->new_master_port("irq", &this->irq);
  this->new_master_port("tcdm", &this->tcdm_port );

}

// The `hwpe_slave` member function models an access to the HWPE SLAVE interface
vp::IoReqStatus Hwpe::hwpe_slave(vp::Block *__this, vp::IoReq *req)
{
  Hwpe *_this = (Hwpe *)__this;
  _this->trace.msg("Received request (addr: 0x%x, size: 0x%x, is_write: %d, data: 0x%x\n", req->get_addr(), req->get_size(), req->get_is_write(), *(uint32_t *)(req->get_data()));
  return vp::IO_REQ_OK;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
  return new Hwpe(config);
}