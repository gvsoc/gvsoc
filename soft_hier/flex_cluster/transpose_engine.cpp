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
 *      Here we only support (No Compute/ INT16 / UINT16 / FP16 ) for matrix multiply
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <list>
#include <queue>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

/****************************************************
*                   Type Definition                 *
****************************************************/

enum transpose_engine_state {
    IDLE,
    START,
    LOADTILE,
    STORETILE,
    FINISHED,
    ACKNOWLEDGE
};

/*****************************************************
*                   Class Definition                 *
*****************************************************/


class TransposeEngine : public vp::Component
{

public:
    TransposeEngine(vp::ComponentConf &config);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    void tranposition();
    uint32_t next_iteration();

    vp::Trace           trace;
    vp::IoSlave         input_itf;
    vp::IoMaster        tcdm_itf;

    //transpose_engine fsm
    vp::IoReq *         core_query;
    vp::IoReq*          tcdm_req;
    vp::ClockEvent *    fsm_event;
    vp::reg_32          state;
    uint32_t            n_row_per_tile;
    uint32_t            n_col_per_tile;
    uint32_t            fsm_counter;
    uint32_t            fsm_timestamp;
    std::queue<uint32_t> pending_req_queue;

    //transpose_engine configuration
    uint32_t            tcdm_bank_width;
    uint32_t            tcdm_bank_number;
    uint32_t            queue_depth;
    uint32_t            buffer_dim;
    uint32_t            bandwidth;
    uint32_t            m_size;
    uint32_t            n_size;
    uint32_t            x_addr;
    uint32_t            y_addr;
    uint32_t            elem_size;
    uint32_t            tile_dim;
    uint32_t            row_iter;
    uint32_t            col_iter;
    uint32_t            max_row_iter;
    uint32_t            max_col_iter;

    //transpose_engine buffer
    uint8_t *           access_buffer;
    uint8_t *           scratch_buffer;
    uint8_t *           transposed_buffer;
};

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new TransposeEngine(config);
}

TransposeEngine::TransposeEngine(vp::ComponentConf &config)
    : vp::Component(config)
{
    //Initialize interface
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->input_itf.set_req_meth(&TransposeEngine::req);
    this->new_slave_port("input", &this->input_itf);
    this->new_master_port("tcdm", &this->tcdm_itf);

    //Initialize configuration
    this->tcdm_bank_width   = get_js_config()->get("tcdm_bank_width")->get_int();
    this->tcdm_bank_number  = get_js_config()->get("tcdm_bank_number")->get_int();
    this->queue_depth       = get_js_config()->get("queue_depth")->get_int();
    this->buffer_dim        = get_js_config()->get("buffer_dim")->get_int();
    this->bandwidth         = this->tcdm_bank_width * this->tcdm_bank_number;
    this->m_size            = 0;
    this->n_size            = 0;
    this->x_addr            = 0;
    this->y_addr            = 0;
    this->elem_size         = 2;
    this->tile_dim          = 0;
    this->row_iter          = 0;
    this->col_iter          = 0;
    this->max_row_iter      = 0;
    this->max_col_iter      = 0;

    //Initialize Buffers
    this->access_buffer     = new uint8_t[this->buffer_dim * 2];
    this->scratch_buffer    = new uint8_t[this->buffer_dim * this->buffer_dim];
    this->transposed_buffer = new uint8_t[this->buffer_dim * this->buffer_dim];

    //Initialize FSM
    this->state.set(IDLE);
    this->core_query        = NULL;
    this->tcdm_req          = this->tcdm_itf.req_new(0, 0, 0, 0);
    this->fsm_event         = this->event_new(&TransposeEngine::fsm_handler);
    this->n_row_per_tile    = 0;
    this->n_col_per_tile    = 0;
    this->fsm_counter       = 0;
    this->fsm_timestamp     = 0;

    this->trace.msg("[TransposeEngine] Model Initialization Done!\n");
}

void TransposeEngine::tranposition()
{
    if (this->elem_size == 1)
    {
        uint8_t * src_buf = (uint8_t *) this->scratch_buffer;
        uint8_t * dst_buf = (uint8_t *) this->transposed_buffer;
        for (int i = 0; i < this->tile_dim; ++i)
        {
            for (int j = 0; j < this->tile_dim; ++j)
            {
                dst_buf[i*this->tile_dim + j] = src_buf[j*this->tile_dim + i];
            }
        }
    } else
    if (this->elem_size == 2)
    {
        uint16_t * src_buf = (uint16_t *) this->scratch_buffer;
        uint16_t * dst_buf = (uint16_t *) this->transposed_buffer;
        for (int i = 0; i < this->tile_dim; ++i)
        {
            for (int j = 0; j < this->tile_dim; ++j)
            {
                dst_buf[i*this->tile_dim + j] = src_buf[j*this->tile_dim + i];
            }
        }
    } else {
        this->trace.fatal("[TransposeEngine][tranposition] Not support for elem_size of %d\n", this->elem_size);
    }
}

uint32_t TransposeEngine::next_iteration()
{
    this->col_iter += 1;
    if (this->col_iter >= this->max_col_iter)
    {
        this->col_iter = 0;
        this->row_iter += 1;
        if (this->row_iter >= this->max_row_iter)
        {
            return 0;
        }
    }
    return 1;
}

vp::IoReqStatus TransposeEngine::req(vp::Block *__this, vp::IoReq *req)
{
    TransposeEngine *_this = (TransposeEngine *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    if ((is_write == 0) && (offset == 20) && (_this->state.get() == IDLE)){
        /*************************
        *  Asynchronize Trigger  *
        *************************/
        //Sanity Check
        _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine] transpose_engine configuration (M-N): %d, %d\n", _this->m_size, _this->n_size);
        if ((_this->m_size == 0)||(_this->n_size == 0))
        {
            _this->trace.fatal("[TransposeEngine] INVALID transpose_engine configuration (M-N): %d, %d\n", _this->m_size, _this->n_size);
            return vp::IO_REQ_OK;
        }

        //Trigger FSM
        _this->state.set(START);
        _this->event_enqueue(_this->fsm_event, 1);

    } else if ((is_write == 0) && (offset == 24) && (_this->core_query == NULL) && (_this->state.get() != IDLE)){
        /*************************
        *  Asynchronize Waiting  *
        *************************/
        _this->core_query = req;
        return vp::IO_REQ_PENDING;

    } else {
        uint32_t value = *(uint32_t *)data;

        switch (offset) {
            case 0:
                _this->m_size = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine] Set M size 0x%x)\n", value);
                break;
            case 4:
                _this->n_size = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine] Set N size 0x%x)\n", value);
                break;
            case 8:
                _this->x_addr = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine] Set X addr 0x%x)\n", value);
                break;
            case 12:
                _this->y_addr = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine] Set Y addr 0x%x)\n", value);
                break;
            case 16:
                _this->elem_size = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine] Set Elem size 0x%x)\n", value);
                break;
            default:
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine] write to INVALID address\n");
        }
    }

    return vp::IO_REQ_OK;
}


void TransposeEngine::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    TransposeEngine *_this = (TransposeEngine *)__this;

    _this->fsm_timestamp += 1;

    switch (_this->state.get()) {
        case IDLE:
            break;

        case START:
            _this->tile_dim          = _this->buffer_dim / _this->elem_size;
            _this->row_iter          = 0;
            _this->col_iter          = 0;
            _this->max_row_iter      = (_this->m_size + _this->tile_dim - 1) / _this->tile_dim;
            _this->max_col_iter      = (_this->n_size + _this->tile_dim - 1) / _this->tile_dim;
            _this->n_row_per_tile    = _this->m_size > _this->tile_dim ? _this->tile_dim : _this->m_size;
            _this->n_col_per_tile    = _this->n_size > _this->tile_dim ? _this->tile_dim : _this->n_size;
            _this->fsm_counter       = 0;
            _this->state.set(LOADTILE);
            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case LOADTILE:
            //Send Request Process
            if ((_this->fsm_counter < _this->n_row_per_tile) && (_this->pending_req_queue.size() <= _this->queue_depth))
            {
                //Form request
                _this->tcdm_req->init();
                _this->tcdm_req->set_addr(_this->x_addr
                    + _this->row_iter * _this->tile_dim * _this->n_size * _this->elem_size /*row tile offset*/
                    + _this->col_iter * _this->tile_dim                 * _this->elem_size /*col tile offset*/
                    + _this->fsm_counter                * _this->n_size * _this->elem_size /*row cntr offset*/);
                _this->tcdm_req->set_data(_this->access_buffer);
                _this->tcdm_req->set_is_write(0);
                _this->tcdm_req->set_size(_this->n_col_per_tile * _this->elem_size);

                //Send request
                vp::IoReqStatus err = _this->tcdm_itf.req(_this->tcdm_req);
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine][LOADTILE-ij: %0d-%0d] --- Send TCDM req #%d\n", _this->row_iter, _this->col_iter, _this->fsm_counter);

                //Check error
                if (err != vp::IO_REQ_OK) {
                    _this->trace.fatal("[TransposeEngine][LOADTILE] There was an error while reading/writing data\n");
                    return;
                }

                //Process Data
                std::memcpy(&(_this->scratch_buffer[_this->tile_dim * _this->fsm_counter * _this->elem_size]), _this->access_buffer, _this->n_col_per_tile * _this->elem_size);

                //Counte on receiving cycle
                uint32_t receive_stamp = _this->tcdm_req->get_latency() + _this->fsm_timestamp;

                //Push pending request queue
                _this->pending_req_queue.push(receive_stamp);

                //Add counter
                _this->fsm_counter += 1;
            }

            //Recieve Process
            while((_this->pending_req_queue.size()!= 0) && (_this->pending_req_queue.front() <= _this->fsm_timestamp) ){
                _this->pending_req_queue.pop();
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine][LOADTILE-ij: %0d-%0d] ---                           Receive TCDM resp\n", _this->row_iter, _this->col_iter);
            }

            //Jump
            if ((_this->fsm_counter >= _this->n_row_per_tile) && (_this->pending_req_queue.size() == 0))
            {
                _this->tranposition();
                uint32_t swap_tmp       = _this->n_row_per_tile;
                _this->n_row_per_tile   = _this->n_col_per_tile;
                _this->n_col_per_tile   = swap_tmp;
                _this->fsm_counter      = 0;
                _this->fsm_timestamp    = 0;
                _this->state.set(STORETILE);
            } else {
                _this->state.set(LOADTILE);
            }

            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case STORETILE:
            //Send Request Process
            if ((_this->fsm_counter < _this->n_row_per_tile) && (_this->pending_req_queue.size() <= _this->queue_depth))
            {
                //Form request
                _this->tcdm_req->init();
                _this->tcdm_req->set_addr(_this->y_addr
                    + _this->col_iter * _this->tile_dim * _this->m_size * _this->elem_size /*row tile offset*/
                    + _this->row_iter * _this->tile_dim                 * _this->elem_size /*col tile offset*/
                    + _this->fsm_counter                * _this->m_size * _this->elem_size /*row cntr offset*/);
                _this->tcdm_req->set_data(_this->access_buffer);
                _this->tcdm_req->set_is_write(1);
                _this->tcdm_req->set_size(_this->n_col_per_tile * _this->elem_size);

                //Process Data
                std::memcpy(_this->access_buffer, &(_this->transposed_buffer[_this->tile_dim * _this->fsm_counter * _this->elem_size]), _this->n_col_per_tile * _this->elem_size);

                //Send request
                vp::IoReqStatus err = _this->tcdm_itf.req(_this->tcdm_req);
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine][STORETILE-ij: %0d-%0d] --- Send TCDM req #%d\n", _this->row_iter, _this->col_iter, _this->fsm_counter);

                //Check error
                if (err != vp::IO_REQ_OK) {
                    _this->trace.fatal("[TransposeEngine][STORETILE] There was an error while reading/writing data\n");
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
                _this->pending_req_queue.pop();
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine][STORETILE-ij: %0d-%0d] ---                           Receive TCDM resp\n", _this->col_iter, _this->row_iter);
            }

            //Jump
            if ((_this->fsm_counter >= _this->n_row_per_tile) && (_this->pending_req_queue.size() == 0))
            {
                if (_this->next_iteration())
                {
                    _this->n_row_per_tile   = (_this->m_size - _this->row_iter * _this->tile_dim) > _this->tile_dim ? _this->tile_dim : (_this->m_size - _this->row_iter * _this->tile_dim);
                    _this->n_col_per_tile   = (_this->n_size - _this->col_iter * _this->tile_dim) > _this->tile_dim ? _this->tile_dim : (_this->n_size - _this->col_iter * _this->tile_dim);
                    _this->fsm_counter      = 0;
                    _this->fsm_timestamp    = 0;
                    _this->state.set(LOADTILE);
                } else {
                    _this->n_row_per_tile   = 0;
                    _this->n_col_per_tile   = 0;
                    _this->fsm_counter      = 0;
                    _this->fsm_timestamp    = 0;
                    _this->state.set(FINISHED);
                }
            } else {
                _this->state.set(STORETILE);
            }

            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case FINISHED:
            //Reply Stalled Query
            if (_this->core_query == NULL)
            {
                _this->state.set(FINISHED);
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine][FINISHED] Waiting for query!\n");
            } else {
                _this->state.set(ACKNOWLEDGE);
            }
            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case ACKNOWLEDGE:
            _this->core_query->get_resp_port()->resp(_this->core_query);
            _this->core_query = NULL;
            _this->state.set(IDLE);
            _this->trace.msg(vp::Trace::LEVEL_TRACE,"[TransposeEngine][ACKNOWLEDGE] Done!\n");
            break;

        default:
            _this->trace.fatal("[TransposeEngine] INVALID TransposeEngine Status: %d\n", _this->state);
    }
}