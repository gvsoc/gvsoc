/*
 * Copyright (C) 2024 ETH Zurich and University of Bologna
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
 * Author: Chi     Zhang , ETH Zurich (chizhang@iis.ee.ethz.ch)
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <list>
#include <queue>

//Function declearation
std::vector<std::vector<int>> initialize_timing_array(int rows, int cols, int buffer_h, int buffer_w, int tcdms_bw);
std::vector<std::vector<int>> cube_unit_timing_array_calculation_folded_tile(
    int x_row_lefts, int x_row_tiles, int x_col_lefts, int x_col_tiles,
    int w_row_lefts, int w_row_tiles, int w_col_lefts, int w_col_tiles,
    int z_row_lefts, int z_row_tiles, int z_col_lefts, int z_col_tiles,
    int buffer_h, int buffer_w, int tcdms_bw);
int cube_unit_runtime(int cube_height, int cube_width, int cube_pipeline, int cube_elem_size_byte, int tcdm_bank_width_byte, int nb_tcdm_banks, int m_size, int n_size, int k_size);


enum redmule_state {
    IDLE,
    ROUTINE,
    COMPENSATION, 
    FINISHED
};


class LightRedmule : public vp::Component
{

public:
    LightRedmule(vp::ComponentConf &config);

// private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    vp::IoReqStatus send_tcdm_req();

    vp::Trace           trace;
    vp::IoSlave         input_itf;
    vp::IoMaster        tcdm_itf;

    //redmule fsm
    vp::IoReq *         redmule_query;
    vp::IoReq*          tcdm_req;
    vp::ClockEvent *    fsm_event;
    vp::reg_32          state;
    uint32_t            tcdm_block_total;
    uint32_t            fsm_counter;
    uint32_t            fsm_timestamp;
    std::queue<uint32_t> pending_req_queue;

    //redmule configuration
    uint32_t            tcdm_bank_width;
    uint32_t            tcdm_bank_number;
    uint32_t            elem_size;
    uint32_t            ce_height;
    uint32_t            ce_width;
    uint32_t            ce_pipe;
    uint32_t            queue_depth;
    uint32_t            bandwidth;

    //redmule registers
    uint32_t            m_size;
    uint32_t            n_size;
    uint32_t            k_size;
    uint32_t            x_addr;
    uint32_t            y_addr;
    uint32_t            z_addr;

    //redmule buffer
    uint8_t *           access_buffer;
    
};

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new LightRedmule(config);
}

LightRedmule::LightRedmule(vp::ComponentConf &config)
    : vp::Component(config)
{
    //Initialize interface
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->input_itf.set_req_meth(&LightRedmule::req);
    this->new_slave_port("input", &this->input_itf);
    this->new_master_port("tcdm", &this->tcdm_itf);
    
    
    //Initialize configuration
    this->tcdm_bank_width   = get_js_config()->get("tcdm_bank_width")->get_int();
    this->tcdm_bank_number  = get_js_config()->get("tcdm_bank_number")->get_int();
    this->elem_size         = get_js_config()->get("elem_size")->get_int();
    this->ce_height         = get_js_config()->get("ce_height")->get_int();
    this->ce_width          = get_js_config()->get("ce_width")->get_int();
    this->ce_pipe           = get_js_config()->get("ce_pipe")->get_int();
    this->queue_depth       = get_js_config()->get("queue_depth")->get_int();
    this->bandwidth         = this->tcdm_bank_width * this->tcdm_bank_number;

    //Initialize registers
    this->m_size            = 4;
    this->n_size            = 4;
    this->k_size            = 4;
    this->x_addr            = 0;
    this->y_addr            = 0;
    this->z_addr            = 0;

    //Initialize Buffers
    this->access_buffer     = new uint8_t[this->bandwidth * 2];

    //Initialize FSM
    this->state.set(IDLE);
    this->redmule_query     = NULL;
    this->tcdm_req          = this->tcdm_itf.req_new(0, 0, 0, 0);
    this->fsm_event         = this->event_new(&LightRedmule::fsm_handler);
    this->tcdm_block_total  = 0;
    this->fsm_counter       = 0;
    this->fsm_timestamp     = 0;
}

vp::IoReqStatus LightRedmule::req(vp::Block *__this, vp::IoReq *req)
{
    LightRedmule *_this = (LightRedmule *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    _this->trace.msg("[LightRedmule] access (offset: 0x%x, size: 0x%x, is_write: %d, data:%x)\n", offset, size, is_write, *(uint32_t *)data);

    if ((is_write == 0) && (_this->redmule_query == NULL))
    {
        //Sanity Check
        _this->trace.msg("[LightRedmule] redmule configuration (M-N-K): %d, %d, %d\n", _this->m_size, _this->n_size, _this->k_size);
        if ((_this->m_size == 0)||(_this->n_size == 0)||(_this->k_size == 0))
        {
            _this->trace.fatal("[LightRedmule] INVALID redmule configuration (M-N-K): %d, %d, %d\n", _this->m_size, _this->n_size, _this->k_size);
            return vp::IO_REQ_OK;
        }

        //Trigger FSM
        _this->state.set(IDLE);
        _this->tcdm_block_total =   (_this->m_size * _this->n_size * _this->elem_size + _this->bandwidth - 1)/_this->bandwidth +
                                    (_this->n_size * _this->k_size * _this->elem_size + _this->bandwidth - 1)/_this->bandwidth +
                                    (_this->m_size * _this->k_size * _this->elem_size + _this->bandwidth - 1)/_this->bandwidth +
                                    (_this->m_size * _this->k_size * _this->elem_size + _this->bandwidth - 1)/_this->bandwidth;
        _this->fsm_counter      = 0;
        _this->fsm_timestamp    = 0;
        _this->event_enqueue(_this->fsm_event, 1);
        _this->trace.msg("[LightRedmule] Need to access %d tcdm blocks\n", _this->tcdm_block_total);

        //Save Query
        _this->redmule_query = req;
        return vp::IO_REQ_PENDING;
    } else {
        uint32_t value = *(uint32_t *)data;

        switch (offset) {
            case 0:
                _this->m_size = value;
                break;
            case 4:
                _this->n_size = value;
                break;
            case 8:
                _this->k_size = value;
                break;
            case 12:
                _this->x_addr = value;
                break;
            case 16:
                _this->y_addr = value;
                break;
            case 20:
                _this->z_addr = value;
                break;
            case 32:
                _this->trace.msg("[LightRedmule] write status\n");
                break;
            default:
                _this->trace.msg("[LightRedmule] write to INVALID address\n");
        }
    }

    return vp::IO_REQ_OK;
}

vp::IoReqStatus LightRedmule::send_tcdm_req()
{
    return this->tcdm_itf.req(this->tcdm_req);
}

void LightRedmule::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    LightRedmule *_this = (LightRedmule *)__this;

    _this->fsm_timestamp += 1;

    switch (_this->state.get()) {
        case IDLE:
            _this->state.set(ROUTINE);
            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case ROUTINE:
            
            //Send Request Process
            if ((_this->fsm_counter < _this->tcdm_block_total) && (_this->pending_req_queue.size() <= _this->queue_depth))
            {
                //Form request
                _this->tcdm_req->set_addr(0);
                _this->tcdm_req->set_data(_this->access_buffer);
                _this->tcdm_req->set_size(_this->bandwidth);

                //Send request
                vp::IoReqStatus err = _this->send_tcdm_req();
                _this->trace.msg("[LightRedmule] --- Send TCDM req #%d\n",_this->fsm_counter);

                //Check error
                if (err != vp::IO_REQ_OK) {
                    _this->trace.fatal("[LightRedmule] There was an error while reading/writing data\n");
                    return;
                }

                //Counte on receiving cycle
                uint32_t receive_stamp = _this->tcdm_req->get_latency() + _this->fsm_timestamp;

                //Push pending request queue
                _this->pending_req_queue.push(receive_stamp);

                //Add counter
                _this->fsm_counter += 1;
            }

            //Recieve Process
            while((_this->pending_req_queue.size()!= 0) && (_this->pending_req_queue.front() <= _this->fsm_timestamp) ){
                _this->trace.msg("[LightRedmule] ---                           Receive TCDM resp\n");
                _this->pending_req_queue.pop();
            }

            //Jump
            if ((_this->fsm_counter >= _this->tcdm_block_total) && (_this->pending_req_queue.size() == 0))
            {
                int modeled_runtime = cube_unit_runtime(
                                        _this->ce_height,       /*cube_height*/
                                        _this->ce_width,        /*cube_width*/
                                        _this->ce_pipe,         /*cube_pipeline*/
                                        _this->elem_size,       /*cube_elem_size_byte*/
                                        _this->tcdm_bank_width, /*tcdm_bank_width_byte*/
                                        _this->tcdm_bank_number,/*nb_tcdm_banks*/
                                        _this->m_size,
                                        _this->n_size,
                                        _this->k_size);
                if (_this->fsm_timestamp >= modeled_runtime)
                {
                    _this->state.set(FINISHED);
                    _this->trace.msg("[LightRedmule] TCDM Access Time(%d) > Model Time(%d)\n", _this->fsm_timestamp, modeled_runtime);
                } else {
                    _this->state.set(COMPENSATION);
                    _this->fsm_counter = modeled_runtime - _this->fsm_timestamp;
                    _this->trace.msg("[LightRedmule] Model Time(%d) > TCDM Access Time(%d)\n", modeled_runtime, _this->fsm_timestamp);
                }
            } else {
                _this->state.set(ROUTINE);
            }

            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case COMPENSATION:
            _this->fsm_counter -= 1;
            if (_this->fsm_counter == 0)
            {
                _this->state.set(FINISHED);
            } else {
                _this->state.set(COMPENSATION);
            }

            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case FINISHED:
            //Reply Stalled Query
            if (_this->redmule_query == NULL)
            {
                _this->trace.fatal("[LightRedmule] INVALID RedMule Query\n");
            } else {
                _this->trace.msg("[LightRedmule] Finished !!!!!!\n");
                _this->redmule_query->get_resp_port()->resp(_this->redmule_query);
                _this->redmule_query = NULL;
            }

            _this->state.set(IDLE);
            break;

        default:
            _this->trace.fatal("[LightRedmule] INVALID RedMule Status: %d\n", _this->state);
    }
}


/**********************************
* RedMule Function Cycle Modeling *
**********************************/

std::vector<std::vector<int>> initialize_timing_array(int rows, int cols, int buffer_h, int buffer_w, int tcdms_bw) {
    std::vector<std::vector<int>> timing_array(rows, std::vector<int>(cols, 0));
    int base_value = (buffer_w * buffer_h) / tcdms_bw;
    int remainder = (buffer_w * buffer_h) % tcdms_bw;
    int increment = base_value + (remainder > 0 ? 1 : 0);
    for (int col = 0; col < rows; col++) {
        for (int row = 0; row < cols; row++) {
            timing_array[col][row] = increment;
        }
    }
    return timing_array;
}

std::vector<std::vector<int>> cube_unit_timing_array_calculation_folded_tile(
    int x_row_lefts, int x_row_tiles, int x_col_lefts, int x_col_tiles,
    int w_row_lefts, int w_row_tiles, int w_col_lefts, int w_col_tiles,
    int z_row_lefts, int z_row_tiles, int z_col_lefts, int z_col_tiles,
    int buffer_h, int buffer_w, int tcdms_bw) {

    std::vector<std::vector<int>> y_access_timing_array = initialize_timing_array(z_col_tiles, z_row_tiles, buffer_h, buffer_w, tcdms_bw);
    std::vector<std::vector<int>> x_access_timing_array = initialize_timing_array(x_col_tiles, x_row_tiles, buffer_h, buffer_w, tcdms_bw);
    std::vector<std::vector<int>> w_access_timing_array = initialize_timing_array(w_col_tiles, w_row_tiles, buffer_w, buffer_w, tcdms_bw);
    std::vector<std::vector<int>> z_access_timing_array = initialize_timing_array(z_col_tiles, z_row_tiles, buffer_h, buffer_w, tcdms_bw);

    if (z_row_lefts > 0) {
        for (int col = 0; col < z_col_tiles; col++) {
            int val = (z_row_lefts * buffer_h) / tcdms_bw + ((z_row_lefts * buffer_h) % tcdms_bw > 0 ? 1 : 0);
            y_access_timing_array[col][z_row_tiles - 1] = val;
            z_access_timing_array[col][z_row_tiles - 1] = val;
        }
    }

    if (x_row_lefts > 0) {
        for (int col = 0; col < x_col_tiles; col++) {
            int val = (x_row_lefts * buffer_h) / tcdms_bw + ((x_row_lefts * buffer_h) % tcdms_bw > 0 ? 1 : 0);
            x_access_timing_array[col][x_row_tiles - 1] = val;
        }
    }

    if (w_row_lefts > 0) {
        for (int col = 0; col < w_col_tiles; col++) {
            int val = (w_row_lefts * buffer_w) / tcdms_bw + ((w_row_lefts * buffer_w) % tcdms_bw > 0 ? 1 : 0);
            w_access_timing_array[col][w_row_tiles - 1] = val;
        }
    }

    if (z_col_lefts > 0) {
        for (int row = 0; row < z_row_tiles; row++) {
            int val = (buffer_w * z_col_lefts) / tcdms_bw + ((buffer_w * z_col_lefts) % tcdms_bw > 0 ? 1 : 0);
            y_access_timing_array[z_col_tiles - 1][row] = val;
            z_access_timing_array[z_col_tiles - 1][row] = val;
        }
    }

    if (x_col_lefts > 0) {
        for (int row = 0; row < x_row_tiles; row++) {
            int val = (buffer_w * x_col_lefts) / tcdms_bw + ((buffer_w * x_col_lefts) % tcdms_bw > 0 ? 1 : 0);
            x_access_timing_array[x_col_tiles - 1][row] = val;
        }
    }

    if (w_col_lefts > 0) {
        for (int row = 0; row < w_row_tiles; row++) {
            int val = (buffer_w * w_col_lefts) / tcdms_bw + ((buffer_w * w_col_lefts) % tcdms_bw > 0 ? 1 : 0);
            w_access_timing_array[w_col_tiles - 1][row] = val;
        }
    }

    if (z_row_lefts > 0 && z_col_lefts > 0) {
        int val = (z_col_lefts * z_row_lefts) / tcdms_bw + ((z_col_lefts * z_row_lefts) % tcdms_bw > 0 ? 1 : 0);
        y_access_timing_array[z_col_tiles - 1][z_row_tiles - 1] = val;
        z_access_timing_array[z_col_tiles - 1][z_row_tiles - 1] = val;
    }

    if (x_row_lefts > 0 && x_col_lefts > 0) {
        int val = (x_col_lefts * x_row_lefts) / tcdms_bw + ((x_col_lefts * x_row_lefts) % tcdms_bw > 0 ? 1 : 0);
        x_access_timing_array[x_col_tiles - 1][x_row_tiles - 1] = val;
    }

    if (w_row_lefts > 0 && w_col_lefts > 0) {
        int val = (w_col_lefts * w_row_lefts) / tcdms_bw + ((w_col_lefts * w_row_lefts) % tcdms_bw > 0 ? 1 : 0);
        w_access_timing_array[w_col_tiles - 1][w_row_tiles - 1] = val;
    }

    return y_access_timing_array;
}

int cube_unit_runtime(int cube_height, int cube_width, int cube_pipeline, int cube_elem_size_byte, int tcdm_bank_width_byte, int nb_tcdm_banks, int m_size, int n_size, int k_size) {
    int buffer_h = cube_height;
    int buffer_w = cube_width * (cube_pipeline + 1);
    int tcdms_bw = tcdm_bank_width_byte * nb_tcdm_banks / cube_elem_size_byte;

    int x_row_lefts = n_size % buffer_w;
    int x_row_tiles = n_size / buffer_w + (x_row_lefts > 0 ? 1 : 0);

    int x_col_lefts = m_size % buffer_h;
    int x_col_tiles = m_size / buffer_h + (x_col_lefts > 0 ? 1 : 0);

    int w_row_lefts = k_size % buffer_w;
    int w_row_tiles = k_size / buffer_w + (w_row_lefts > 0 ? 1 : 0);

    int w_col_lefts = x_row_lefts;
    int w_col_tiles = x_row_tiles;

    int z_row_lefts = w_row_lefts;
    int z_row_tiles = w_row_tiles;

    int z_col_lefts = x_col_lefts;
    int z_col_tiles = x_col_tiles;

    auto y_access_timing_array = cube_unit_timing_array_calculation_folded_tile(
        x_row_lefts, x_row_tiles, x_col_lefts, x_col_tiles,
        w_row_lefts, w_row_tiles, w_col_lefts, w_col_tiles,
        z_row_lefts, z_row_tiles, z_col_lefts, z_col_tiles,
        buffer_h, buffer_w, tcdms_bw
    );

    int runtime = 0;

    int preload_time = y_access_timing_array[0][0];
    runtime += preload_time;

    int z_store_time = y_access_timing_array[0][0];
    for (int row = 0; row < z_row_tiles; row++) {
        for (int col = 0; col < z_col_tiles; col++) {
            for (int i = 0; i < x_row_tiles; i++) {
                int cube_unit_mac_time = buffer_w * (cube_pipeline + 1);

                int cube_unit_access_time = 0;

                int _row = row;
                int _col = col;
                int _i = i + 1;
                if (_i >= x_row_tiles) {
                    _i = 0;
                    _col = col + 1;
                    if (_col >= z_col_tiles) {
                        _col = 0;
                        _row = row + 1;
                    }
                }

                if (_row < z_row_tiles) {
                    cube_unit_access_time = y_access_timing_array[_col][_i];
                    if (_i == 0) {
                        cube_unit_access_time += y_access_timing_array[_col][_row];
                    }
                }

                if (i == 0 && (row != 0 || col != 0)) {
                    cube_unit_access_time += z_store_time;
                    z_store_time = y_access_timing_array[col][row];
                }

                if (cube_unit_mac_time >= cube_unit_access_time) {
                    runtime += cube_unit_mac_time;
                } else {
                    runtime += cube_unit_access_time;
                }
            }
        }
    }

    int final_store_time = 0;
    int cube_unit_store_time_limit = buffer_w;
    int cube_unit_store_time = y_access_timing_array[z_col_tiles - 1][z_row_tiles - 1];
    if (cube_unit_store_time > cube_unit_store_time_limit) {
        final_store_time += cube_unit_store_time;
    } else {
        final_store_time += cube_unit_store_time_limit;
    }
    runtime += final_store_time;

    return runtime;
}