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
 *          Arpan Suravi Prasad, ETH Zurich (prasadar@iis.ee.ethz.ch)
 */
#ifndef __HWPE_REG_CONFIG_H
#define __HWPE_REG_CONFIG_H
#include "params.hpp"
template <typename HwpeType>
class RegConfigManager
{
  private:
    int regfile_[HWPE_NB_REG];
    int job_running_;
    HwpeType* hwpe_instance_;

  public:
    RegConfigManager(){};
    RegConfigManager(HwpeType* hwpe) {
      hwpe_instance_ = hwpe;
    }

  int regfile_read(int addr) {
    if(addr < HWPE_NB_REG) 
      return regfile_[addr];
    else
      this->hwpe_instance_->trace.fatal("Exceeds register index\n");
    return -1;
  }

  void regfile_write(int addr, int value) {
    if(addr < HWPE_NB_REG) 
      regfile_[addr] = value;
    else 
      this->hwpe_instance_->trace.fatal("Exceeds register index\n");
  }

};

#endif