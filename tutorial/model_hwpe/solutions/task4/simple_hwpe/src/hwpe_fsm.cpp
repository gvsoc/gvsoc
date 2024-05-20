
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
#include <type_traits>

#include <string>
#include <sstream>
void Hwpe::FsmStartHandler(vp::Block *__this, vp::ClockEvent *event) {// makes sense to move it to task manager
  Hwpe *_this = (Hwpe *)__this;
  _this->state.set(START);
  //////////////////////////////// SOLUTION - 1 ///////////////////////////
  // assign the register values to reg_config_ . Hint: use get_reg_config() of regconfig_manager_instance
  _this->reg_config_=_this->regconfig_manager_instance.get_reg_config();

  _this->fsm_loop();
}
void Hwpe::FsmHandler(vp::Block *__this, vp::ClockEvent *event) {
  Hwpe *_this = (Hwpe *)__this;
  _this->fsm_loop();
}
void Hwpe::FsmEndHandler(vp::Block *__this, vp::ClockEvent *event) {// makes sense to move it to task manager
  Hwpe *_this = (Hwpe *)__this;
  _this->state.set(IDLE);
}

void Hwpe::fsm_loop() {
  auto latency = 0;
  do {
    latency = this->fsm();
  } while(latency == 0 && state.get() != END);
  if(state.get() == END && !this->fsm_end_event->is_enqueued()) {
    this->fsm_end_event->enqueue(latency);
  }
  else if (!this->fsm_event->is_enqueued()) {
    this->fsm_event->enqueue(latency);
  }
}

int Hwpe::fsm() {
  int latency = 0;
  auto state_next = this->state.get();
  switch(this->state.get()) {
    case START:
      state_next = LOAD_INPUT; 
      //////////////////////////////// SOLUTION - 2 ///////////////////////////
      // Print register values . Hint use print_reg() function of RegConfigManager
      this->regconfig_manager_instance.print_reg();

      break;
    case LOAD_INPUT:
      //////////////////////////////// SOLUTION - 3 ///////////////////////////
      // Call input_load() function 
      latency = this->input_load();

      if(this->input.iteration == this->input.count)
        state_next = WEIGHT_OFFSET; 
      break;
    case WEIGHT_OFFSET:
      latency = this->weight_offset();
      state_next = COMPUTE; 
      break;
    case COMPUTE:
      latency = this->weight_load();
      this->weight_layout();
      latency += this->compute();
      state_next = STORE_OUTPUT; 
      break;
    case STORE_OUTPUT:
      latency = this->output_store();
      state_next = END;
      latency = 1;
      break;
    case END:
      break;
  }
}