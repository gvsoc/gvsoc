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
 * Note:
 *      Here we ignore real calculation for matrix multiply
 *      in order to accelerate simulation
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <list>
#include <queue>


enum redmule_state {
    IDLE,
    PRELOAD,
    ROUTINE,
    STORING,
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
    void init_redmule_meta_data();
    uint32_t next_iteration();
    uint32_t get_routine_access_block_number();
    uint32_t get_preload_access_block_number();
    uint32_t get_storing_access_block_number();

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
    int64_t             timer_start;
    int64_t             total_runtime;
    int64_t             num_matmul;

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

    //redmule meta data
    uint32_t            x_row_tiles;
    uint32_t            x_row_lefts;
    uint32_t            x_col_tiles;
    uint32_t            x_col_lefts;
    uint32_t            w_row_tiles;
    uint32_t            w_row_lefts;
    uint32_t            w_col_tiles;
    uint32_t            w_col_lefts;
    uint32_t            z_row_tiles;
    uint32_t            z_row_lefts;
    uint32_t            z_col_tiles;
    uint32_t            z_col_lefts;
    uint32_t            iter_i;
    uint32_t            iter_j;
    uint32_t            iter_k;

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

    //Initialize redmule meta data
    this->x_row_tiles       = 0;
    this->x_row_lefts       = 0;
    this->x_col_tiles       = 0;
    this->x_col_lefts       = 0;
    this->w_row_tiles       = 0;
    this->w_row_lefts       = 0;
    this->w_col_tiles       = 0;
    this->w_col_lefts       = 0;
    this->z_row_tiles       = 0;
    this->z_row_lefts       = 0;
    this->z_col_tiles       = 0;
    this->z_col_lefts       = 0;
    this->iter_i            = 0;
    this->iter_j            = 0;
    this->iter_k            = 0;

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
    this->timer_start       = 0;
    this->total_runtime     = 0;
    this->num_matmul        = 0; 


}

void LightRedmule::init_redmule_meta_data(){
    int buffer_h = this->ce_height;
    int buffer_w = this->ce_width * (this->ce_pipe + 1);

    this->x_row_lefts = this->n_size % buffer_w;
    this->x_row_tiles = this->n_size / buffer_w + (this->x_row_lefts > 0 ? 1 : 0);

    this->x_col_lefts = this->m_size % buffer_h;
    this->x_col_tiles = this->m_size / buffer_h + (this->x_col_lefts > 0 ? 1 : 0);

    this->w_row_lefts = this->k_size % buffer_w;
    this->w_row_tiles = this->k_size / buffer_w + (this->w_row_lefts > 0 ? 1 : 0);

    this->w_col_lefts = this->x_row_lefts;
    this->w_col_tiles = this->x_row_tiles;
    this->z_row_lefts = this->w_row_lefts;
    this->z_row_tiles = this->w_row_tiles;
    this->z_col_lefts = this->x_col_lefts;
    this->z_col_tiles = this->x_col_tiles;
}

uint32_t LightRedmule::next_iteration(){
    this->iter_k += 1;
    if (this->iter_k == this->x_row_tiles)
    {
        this->iter_k = 0;
        this->iter_j += 1;
        if (this->iter_j == this->z_row_tiles)
        {
            this->iter_j = 0;
            this->iter_i += 1;
            if (this->iter_i == this->z_col_tiles)
            {
                return 1;
            }
        }
    }
    return 0;
}

uint32_t LightRedmule::get_routine_access_block_number(){
    uint32_t total_rows         = 0;
    uint32_t is_last_iteration  = (this->iter_i == (this->z_col_tiles - 1)) && (this->iter_j == (this->z_row_tiles - 1)) && (this->iter_k == (this->x_row_tiles - 1));
    uint32_t is_first_iteration = (this->iter_i == 0) && (this->iter_j == 0) && (this->iter_k == 0);
    uint32_t tcdms_bw           = this->bandwidth / this->elem_size;
    uint32_t block_per_row      = (this->ce_width * (this->ce_pipe + 1) + tcdms_bw - 1) / tcdms_bw;
    // W block
    total_rows += this->ce_width * (this->ce_pipe + 1);

    // X block
    if (is_last_iteration == 0)
    {
        total_rows += this->ce_height;
    }

    // Y block
    if (this->iter_k == (this->x_row_tiles - 1) && (is_last_iteration == 0))
    {
        total_rows += this->ce_height;
    }

    // Z block
    if ((this->iter_k == 0) && (is_first_iteration == 0))
    {
        total_rows += this->ce_height;
    }

    return block_per_row * total_rows;

}

uint32_t LightRedmule::get_preload_access_block_number(){
    uint32_t total_rows         = 0;
    uint32_t tcdms_bw           = this->bandwidth / this->elem_size;
    uint32_t block_per_row      = (this->ce_width * (this->ce_pipe + 1) + tcdms_bw - 1) / tcdms_bw;

    // X & Y block
    total_rows = 2 * this->ce_height;

    return block_per_row * total_rows;

}

uint32_t LightRedmule::get_storing_access_block_number(){
    uint32_t total_rows         = 0;
    uint32_t tcdms_bw           = this->bandwidth / this->elem_size;
    uint32_t block_per_row      = (this->ce_width * (this->ce_pipe + 1) + tcdms_bw - 1) / tcdms_bw;

    // X & Y block
    total_rows = this->ce_height;

    return block_per_row * total_rows;

}

vp::IoReqStatus LightRedmule::req(vp::Block *__this, vp::IoReq *req)
{
    LightRedmule *_this = (LightRedmule *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] access (offset: 0x%x, size: 0x%x, is_write: %d, data:%x)\n", offset, size, is_write, *(uint32_t *)data);

    if ((is_write == 0) && (_this->redmule_query == NULL))
    {
        //Sanity Check
        _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] redmule configuration (M-N-K): %d, %d, %d\n", _this->m_size, _this->n_size, _this->k_size);
        if ((_this->m_size == 0)||(_this->n_size == 0)||(_this->k_size == 0))
        {
            _this->trace.fatal("[LightRedmule] INVALID redmule configuration (M-N-K): %d, %d, %d\n", _this->m_size, _this->n_size, _this->k_size);
            return vp::IO_REQ_OK;
        }

        //Initilaize redmule meta data
        _this->init_redmule_meta_data();

        //Trigger FSM
        _this->state.set(IDLE);
        _this->tcdm_block_total = 0;
        _this->fsm_counter      = 0;
        _this->fsm_timestamp    = 0;
        _this->timer_start      = _this->time.get_time();
        _this->event_enqueue(_this->fsm_event, 1);

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
            _this->state.set(PRELOAD);
            _this->tcdm_block_total = _this->get_preload_access_block_number();
            _this->fsm_counter      = 0;
            _this->fsm_timestamp    = 0;
            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case PRELOAD:
            //Send Request Process
            if ((_this->fsm_counter < _this->tcdm_block_total) && (_this->pending_req_queue.size() <= _this->queue_depth))
            {
                //Form request
                _this->tcdm_req->set_addr(0);
                _this->tcdm_req->set_data(_this->access_buffer);
                _this->tcdm_req->set_size(_this->bandwidth);

                //Send request
                vp::IoReqStatus err = _this->send_tcdm_req();
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Preload] --- Send TCDM req #%d\n",_this->fsm_counter);

                //Check error
                if (err != vp::IO_REQ_OK) {
                    _this->trace.fatal("[LightRedmule][Preload] There was an error while reading/writing data\n");
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
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Preload] ---                           Receive TCDM resp\n");
                _this->pending_req_queue.pop();
            }

            //Jump
            if ((_this->fsm_counter >= _this->tcdm_block_total) && (_this->pending_req_queue.size() == 0))
            {
                _this->tcdm_block_total = _this->get_routine_access_block_number();
                _this->fsm_counter      = 0;
                _this->fsm_timestamp    = 0;
                _this->state.set(ROUTINE);
            } else {
                _this->state.set(PRELOAD);
            }

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
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] --- Send TCDM req #%d\n", _this->iter_i, _this->iter_j, _this->iter_k, _this->fsm_counter);

                //Check error
                if (err != vp::IO_REQ_OK) {
                    _this->trace.fatal("[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] There was an error while reading/writing data\n", _this->iter_i, _this->iter_j, _this->iter_k);
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
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] ---                           Receive TCDM resp\n", _this->iter_i, _this->iter_j, _this->iter_k);
                _this->pending_req_queue.pop();
            }

            //Jump
            if ((_this->fsm_counter >= _this->tcdm_block_total) && (_this->pending_req_queue.size() == 0))
            {
                int modeled_runtime = _this->ce_width * (_this->ce_pipe + 1) * (_this->ce_pipe + 1);

                // Compensation
                if (_this->fsm_timestamp >= modeled_runtime)
                {
                    _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] TCDM Access Time(%d) > Model Time(%d)\n", _this->iter_i, _this->iter_j, _this->iter_k, _this->fsm_timestamp, modeled_runtime);
                    _this->event_enqueue(_this->fsm_event, 1);
                } else {
                    _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] Model Time(%d) > TCDM Access Time(%d)\n",  _this->iter_i, _this->iter_j, _this->iter_k, modeled_runtime, _this->fsm_timestamp);
                    _this->event_enqueue(_this->fsm_event, modeled_runtime - _this->fsm_timestamp + 1);
                }

                // Next iteration
                _this->fsm_counter      = 0;
                _this->fsm_timestamp    = 0;
                if (_this->next_iteration() == 0)
                {
                    _this->tcdm_block_total = _this->get_routine_access_block_number();
                    _this->state.set(ROUTINE);
                } else {
                    _this->tcdm_block_total = _this->get_storing_access_block_number();
                    _this->state.set(STORING);
                }
            } else {
                _this->state.set(ROUTINE);
                _this->event_enqueue(_this->fsm_event, 1);
            }

            break;

        case STORING:

            //Send Request Process
            if ((_this->fsm_counter < _this->tcdm_block_total) && (_this->pending_req_queue.size() <= _this->queue_depth))
            {
                //Form request
                _this->tcdm_req->set_addr(0);
                _this->tcdm_req->set_data(_this->access_buffer);
                _this->tcdm_req->set_size(_this->bandwidth);

                //Send request
                vp::IoReqStatus err = _this->send_tcdm_req();
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Storing] --- Send TCDM req #%d\n",_this->fsm_counter);

                //Check error
                if (err != vp::IO_REQ_OK) {
                    _this->trace.fatal("[LightRedmule][Storing] There was an error while reading/writing data\n");
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
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Storing] ---                           Receive TCDM resp\n");
                _this->pending_req_queue.pop();
            }

            //Jump
            if ((_this->fsm_counter >= _this->tcdm_block_total) && (_this->pending_req_queue.size() == 0))
            {
                _this->tcdm_block_total = 0;
                _this->fsm_counter      = 0;
                _this->fsm_timestamp    = 0;
                _this->state.set(FINISHED);
            } else {
                _this->state.set(STORING);
            }

            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case FINISHED:
            //Reply Stalled Query
            if (_this->redmule_query == NULL)
            {
                _this->trace.fatal("[LightRedmule] INVALID RedMule Query\n");
            } else {
                int64_t start_time_ns   = (_this->timer_start)/1000;
                int64_t end_time_ns     = (_this->time.get_time())/1000;
                int64_t period_ns       = end_time_ns - start_time_ns;
                _this->total_runtime   += period_ns;
                _this->num_matmul      += 1;
                _this->trace.msg("[LightRedmule] Finished : %0d ns ---> %0d ns | period = %0d ns | runtime = %0d ns | matmul id = %0d\n", start_time_ns, end_time_ns, period_ns, _this->total_runtime, _this->num_matmul);
                _this->redmule_query->get_resp_port()->resp(_this->redmule_query);
                _this->redmule_query = NULL;
            }

            _this->state.set(IDLE);
            break;

        default:
            _this->trace.fatal("[LightRedmule] INVALID RedMule Status: %d\n", _this->state);
    }
}


