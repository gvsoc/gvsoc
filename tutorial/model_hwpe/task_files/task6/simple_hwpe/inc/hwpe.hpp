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
#ifndef __HWPE_HPP__
#define __HWPE_HPP__
#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <assert.h>
#include <string>
#include <bitset>
#include "params.hpp"
#include "datatype.hpp"
#include "scalar_buffer.hpp"
#include "linear_buffer.hpp"
#include "binconv.hpp"
#include "column.hpp"
#include "pe_compute_unit.hpp"
#include "regconfig_manager.hpp"
class Hwpe : public vp::Component
{
public:
  Hwpe(vp::ComponentConf &config);
  vp::IoMaster tcdm_port;
  vp::Trace trace;
  vp::reg_32 state;
  vp::IoReq io_req;


  //register configuration instance
  RegConfigManager<Hwpe> regconfig_manager_instance;

private:
  vp::IoSlave cfg_port_;
  vp::WireMaster<bool> irq;

  Regconfig reg_config_;


  Iterator input, weight, compute, output;

  //////////////////// DATAPATH RELATED DATA STRUCTURE ////////////////////

  LinearBuffer<InputType, INPUT_LINEAR_BUFFER_SIZE> input_buffer_;
  LinearBuffer<OutputType, OUTPUT_LINEAR_BUFFER_SIZE> output_buffer_;
  PEComputeUnit<Hwpe, InputType, WeightType, OutputType, COLUMN_PER_PE, BINCONV_PER_COLUMN> pe_instance_;

  std::array<std::array<InputType, BINCONV_PER_COLUMN>, COLUMN_PER_PE> input_layout_;
  std::array<WeightType, BINCONV_PER_COLUMN*COLUMN_PER_PE/8> weight_temp_buf_;
  std::array<std::array<WeightType, BINCONV_PER_COLUMN>,COLUMN_PER_PE> weight_temp_layout_;
  ////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  void clear();
  int  fsm();
  void fsm_loop();

  int64_t input_load();
  void    input_layout();

  int64_t weight_load();
  void    weight_layout();
  
  int64_t weight_offset();
  
  int64_t compute_output();
  int64_t output_store();

  static vp::IoReqStatus hwpe_slave(vp::Block *__this, vp::IoReq *req);

  vp::ClockEvent *fsm_start_event;
  vp::ClockEvent *fsm_event;
  vp::ClockEvent *fsm_end_event;

  static void FsmStartHandler(vp::Block *__this, vp::ClockEvent *event);
  static void FsmHandler(vp::Block *__this, vp::ClockEvent *event);
  static void FsmEndHandler(vp::Block *__this, vp::ClockEvent *event);
};
#endif /* __HWPE_HPP__ */