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
    FINISHED,
    ACKNOWLEDGE
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
    uint32_t tmp_next_addr();
    uint32_t next_addr();
    uint32_t calculate_tile_base_address(uint32_t base, uint32_t stride, uint32_t tile_col, uint32_t tile_row, uint32_t i, uint32_t j);
    uint32_t inc_addr(uint32_t addr, uint32_t stride, uint32_t tile_row);
    uint32_t next_iteration();
    uint32_t get_redmule_array_runtime();
    uint32_t get_routine_access_block_number();
    uint32_t get_preload_access_block_number();
    uint32_t get_storing_access_block_number();
    uint32_t get_routine_to_storing_latency();

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
    uint32_t            redmule_id;
    uint32_t            tcdm_bank_width;
    uint32_t            tcdm_bank_number;
    uint32_t            elem_size;
    uint32_t            ce_height;
    uint32_t            ce_width;
    uint32_t            ce_pipe;
    uint32_t            queue_depth;
    uint32_t            bandwidth;
    uint32_t            fold_tiles_mapping;

    //redmule registers
    uint32_t            m_size;
    uint32_t            n_size;
    uint32_t            k_size;
    uint32_t            x_addr;
    uint32_t            w_addr;
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
    uint32_t            iter_x_addr;
    uint32_t            iter_w_addr;
    uint32_t            iter_y_addr;
    uint32_t            iter_z_addr;
    uint32_t            x_acc_block;
    uint32_t            w_acc_block;
    uint32_t            y_acc_block;
    uint32_t            z_acc_block;
    double              ideal_runtime;

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
    this->redmule_id        = get_js_config()->get("redmule_id")->get_int();
    this->tcdm_bank_width   = get_js_config()->get("tcdm_bank_width")->get_int();
    this->tcdm_bank_number  = get_js_config()->get("tcdm_bank_number")->get_int();
    this->elem_size         = get_js_config()->get("elem_size")->get_int();
    this->ce_height         = get_js_config()->get("ce_height")->get_int();
    this->ce_width          = get_js_config()->get("ce_width")->get_int();
    this->ce_pipe           = get_js_config()->get("ce_pipe")->get_int();
    this->queue_depth       = get_js_config()->get("queue_depth")->get_int();
    this->fold_tiles_mapping= get_js_config()->get("fold_tiles_mapping")->get_int();
    this->bandwidth         = this->tcdm_bank_width * this->tcdm_bank_number;

    //Initialize registers
    this->m_size            = 4;
    this->n_size            = 4;
    this->k_size            = 4;
    this->x_addr            = 0;
    this->w_addr            = 0;
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
    this->iter_x_addr       = 0;
    this->iter_w_addr       = 0;
    this->iter_y_addr       = 0;
    this->iter_z_addr       = 0;
    this->x_acc_block       = 0;
    this->w_acc_block       = 0;
    this->y_acc_block       = 0;
    this->z_acc_block       = 0;
    this->ideal_runtime     = 0;

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
    uint32_t buffer_h = this->ce_height;
    uint32_t buffer_w = this->ce_width * (this->ce_pipe + 1);
    uint32_t buffer_n = this->bandwidth / this->elem_size;

    this->x_row_lefts = this->n_size % buffer_n;
    this->x_row_tiles = this->n_size / buffer_n + (this->x_row_lefts > 0 ? 1 : 0);

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

    this->iter_i = 0;
    this->iter_j = 0;
    this->iter_k = 0;

    this->iter_x_addr = this->x_addr;
    this->iter_w_addr = this->w_addr;
    this->iter_y_addr = this->y_addr;
    this->iter_z_addr = this->z_addr;

    this->x_acc_block = 0;
    this->w_acc_block = 0;
    this->y_acc_block = 0;
    this->z_acc_block = 0;

    this->ideal_runtime = 1.0 * (this->m_size * this->n_size * this->k_size)/( 1.0 * this->ce_height * this->ce_width);
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

uint32_t LightRedmule::calculate_tile_base_address(uint32_t base, uint32_t stride, uint32_t tile_col, uint32_t tile_row, uint32_t i, uint32_t j){
    /*
    *       ----
    *   col |  |
    *       ----
    *        row
    */
    if (this->fold_tiles_mapping)
    {
        uint32_t tiles_per_row = (stride + tile_row - 1)/tile_row;
        return base + (i * tiles_per_row + j) * tile_row * tile_col * this->elem_size;
    } else {
        return base + (i * tile_col * stride + j * tile_row) * this->elem_size;
    }
}

uint32_t LightRedmule::inc_addr(uint32_t addr, uint32_t stride, uint32_t tile_row){
    if (this->fold_tiles_mapping)
    {
        return addr + tile_row * this->elem_size;
    } else {
        return addr + stride * this->elem_size;
    }
}

uint32_t LightRedmule::tmp_next_addr(){
    this->z_addr = (this->z_addr + this->bandwidth) % (this->ce_height * this->bandwidth);
    return this->z_addr;
}

uint32_t LightRedmule::next_addr(){
    uint32_t addr       = 0;
    uint32_t buffer_h   = this->ce_height;
    uint32_t buffer_w   = this->ce_width * (this->ce_pipe + 1);
    uint32_t buffer_n   = this->bandwidth / this->elem_size;

    // Y -> W -> X -> Z
    if (this->y_acc_block > 0)
    {
        addr = this->iter_y_addr;
        this->iter_y_addr = this->inc_addr(addr, this->k_size, buffer_w);
        this->y_acc_block -= 1;
        this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Address] Y tile at 0x%11x | #Y tile left %d\n", addr, this->y_acc_block);
    } else if (this->w_acc_block > 0)
    {
        addr = this->iter_w_addr;
        this->iter_w_addr = this->inc_addr(addr, this->k_size, buffer_w);
        this->w_acc_block -= 1;
        this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Address] W tile at 0x%11x | #W tile left %d\n", addr, this->w_acc_block);
    } else if (this->x_acc_block > 0)
    {
        addr = this->iter_x_addr;
        this->iter_x_addr = this->inc_addr(addr, this->n_size, buffer_n);
        this->x_acc_block -= 1;
        this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Address] X tile at 0x%11x | #X tile left %d\n", addr, this->x_acc_block);
    } else if (this->z_acc_block > 0)
    {
        addr = this->iter_z_addr;
        this->iter_z_addr = this->inc_addr(addr, this->k_size, buffer_w);
        this->z_acc_block -= 1;
        this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Address] Z tile at 0x%11x | #Z tile left %d\n", addr, this->z_acc_block);
    } else {
        this->trace.fatal("[LightRedmule][Address] INVALID redmule address iteration : No tiles to access\n");
    }

    return addr;

}

uint32_t LightRedmule::get_routine_access_block_number(){
    uint32_t total_blocks       = 0;
    uint32_t is_last_iteration  = (this->iter_i == (this->z_col_tiles - 1)) && (this->iter_j == (this->z_row_tiles - 1)) && (this->iter_k == (this->x_row_tiles - 1));
    uint32_t is_first_iteration = (this->iter_i == 0) && (this->iter_j == 0) && (this->iter_k == 0);
    uint32_t buffer_h           = this->ce_height;
    uint32_t buffer_w           = this->ce_width * (this->ce_pipe + 1);
    uint32_t buffer_n           = this->bandwidth / this->elem_size;
    uint32_t tcdms_bw           = buffer_n;
    uint32_t X_block_per_row    = 1;
    uint32_t WYZ_block_per_row  = (this->ce_width * (this->ce_pipe + 1) + tcdms_bw - 1) / tcdms_bw;

    this->x_acc_block = 0;
    this->w_acc_block = 0;
    this->y_acc_block = 0;
    this->z_acc_block = 0;
    

    // W block
    if (this->iter_k == (this->x_row_tiles - 1) && (this->x_row_lefts > 0))
    {
        this->w_acc_block = this->x_row_lefts * WYZ_block_per_row;
    } else {
        this->w_acc_block = tcdms_bw * WYZ_block_per_row;
    }
    total_blocks += this->w_acc_block;
    this->iter_w_addr = this->calculate_tile_base_address(this->w_addr, this->k_size, buffer_n, buffer_w, this->iter_k, this->iter_j);

    // X block
    if (is_last_iteration == 0)
    {
        this->x_acc_block = this->ce_height * X_block_per_row;
        total_blocks += this->x_acc_block;

        //update x tile base address
        uint32_t _i = this->iter_i;
        uint32_t _k = this->iter_k;
        _k += 1;
        if (_k == this->x_row_tiles)
        {
            _k = 0;
            _i += 1;
        }
        this->iter_x_addr = this->calculate_tile_base_address(this->x_addr, this->n_size, buffer_h, buffer_n, _i, _k);
    }

    // Y block
    if (this->iter_k == (this->x_row_tiles - 1) && (is_last_iteration == 0))
    {
        this->y_acc_block = this->ce_height * WYZ_block_per_row;
        total_blocks += this->y_acc_block;

        //update y tile base address
        uint32_t _i = this->iter_i;
        uint32_t _j = this->iter_j;
        _j += 1;
        if (_j == this->z_row_tiles)
        {
            _j = 0;
            _i += 1;
        }
        this->iter_y_addr = this->calculate_tile_base_address(this->y_addr, this->k_size, buffer_h, buffer_w, _i, _j);
    }

    // Z block
    if ((this->iter_k == 0) && (is_first_iteration == 0))
    {
        this->z_acc_block = this->ce_height * WYZ_block_per_row;
        total_blocks += this->z_acc_block;

        //update z tile base address
        uint32_t _i = this->iter_i;
        uint32_t _j = this->iter_j;
        if (_j == 0)
        {
            _j = this->z_row_tiles - 1;
            _i = _i - 1;
        } else {
            _j = _j -1;
        }
        this->iter_z_addr = this->calculate_tile_base_address(this->z_addr, this->k_size, buffer_h, buffer_w, _i, _j);
    }

    return total_blocks;

}

uint32_t LightRedmule::get_preload_access_block_number(){
    uint32_t total_blocks       = 0;
    uint32_t tcdms_bw           = this->bandwidth / this->elem_size;
    uint32_t X_block_per_row    = 1;
    uint32_t WYZ_block_per_row  = (this->ce_width * (this->ce_pipe + 1) + tcdms_bw - 1) / tcdms_bw;

    this->x_acc_block = 0;
    this->w_acc_block = 0;
    this->y_acc_block = 0;
    this->z_acc_block = 0;

    // X & Y block
    total_blocks = this->ce_height * (X_block_per_row + WYZ_block_per_row);
    this->x_acc_block = this->ce_height * X_block_per_row;
    this->y_acc_block = this->ce_height * WYZ_block_per_row;

    return total_blocks;

}

uint32_t LightRedmule::get_storing_access_block_number(){
    uint32_t total_blocks       = 0;
    uint32_t buffer_h           = this->ce_height;
    uint32_t buffer_w           = this->ce_width * (this->ce_pipe + 1);
    uint32_t buffer_n           = this->bandwidth / this->elem_size;
    uint32_t tcdms_bw           = this->bandwidth / this->elem_size;
    uint32_t WYZ_block_per_row  = (this->ce_width * (this->ce_pipe + 1) + tcdms_bw - 1) / tcdms_bw;

    this->x_acc_block = 0;
    this->w_acc_block = 0;
    this->y_acc_block = 0;
    this->z_acc_block = 0;

    // Z block
    total_blocks = this->ce_height * WYZ_block_per_row;
    this->z_acc_block = this->ce_height * WYZ_block_per_row;
    this->iter_z_addr = this->calculate_tile_base_address(this->z_addr, this->k_size, buffer_h, buffer_w, this->z_col_tiles - 1, this->z_row_tiles - 1);

    return total_blocks;

}

uint32_t LightRedmule::get_routine_to_storing_latency(){
    return this->ce_width * (this->ce_pipe + 1);
    // return 0;
}

uint32_t LightRedmule::get_redmule_array_runtime(){
    uint32_t tcdms_bw       = this->bandwidth / this->elem_size;
    uint32_t runtime_unit   = this->ce_width * (this->ce_pipe + 1);
    uint32_t runtime_pices  = 1;
    uint32_t runtime        = tcdms_bw * (this->ce_pipe + 1);
    if (this->iter_k == (this->x_row_tiles - 1) && (this->x_row_lefts > 0))
    {
        runtime = this->x_row_lefts * (this->ce_pipe + 1);
    }
    runtime_pices = (runtime + runtime_unit - 1)/runtime_unit;
    return runtime_pices * runtime_unit;
}

vp::IoReqStatus LightRedmule::req(vp::Block *__this, vp::IoReq *req)
{
    LightRedmule *_this = (LightRedmule *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    // _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] access (offset: 0x%x, size: 0x%x, is_write: %d, data:%x)\n", offset, size, is_write, *(uint32_t *)data);

    if ((is_write == 0) && (offset == 32) && (_this->redmule_query == NULL) && (_this->state.get() == IDLE))
    {
        /************************
        *  Synchronize Trigger  *
        ************************/
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
        _this->state.set(PRELOAD);
        _this->tcdm_block_total = _this->get_preload_access_block_number();
        _this->fsm_counter      = 0;
        _this->fsm_timestamp    = 0;
        _this->timer_start      = _this->time.get_time();
        _this->event_enqueue(_this->fsm_event, 1);

        //Save Query
        _this->redmule_query = req;
        return vp::IO_REQ_PENDING;

    } else if ((is_write == 0) && (offset == 36) && (_this->state.get() == IDLE)){
        /*************************
        *  Asynchronize Trigger  *
        *************************/
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
        _this->state.set(PRELOAD);
        _this->tcdm_block_total = _this->get_preload_access_block_number();
        _this->fsm_counter      = 0;
        _this->fsm_timestamp    = 0;
        _this->timer_start      = _this->time.get_time();
        _this->event_enqueue(_this->fsm_event, 1);

    } else if ((is_write == 0) && (offset == 40) && (_this->redmule_query == NULL) && (_this->state.get() != IDLE)){
        /*************************
        *  Asynchronize Waiting  *
        *************************/
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
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set X addr 0x%x)\n", value);
                break;
            case 16:
                _this->y_addr = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set Y addr 0x%x)\n", value);
                break;
            case 20:
                _this->z_addr = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set Z addr 0x%x)\n", value);
                break;
            case 24:
                _this->w_addr = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set W addr 0x%x)\n", value);
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
            break;

        case PRELOAD:
            //Send Request Process
            if ((_this->fsm_counter < _this->tcdm_block_total) && (_this->pending_req_queue.size() <= _this->queue_depth))
            {
                //Form request
                _this->tcdm_req->init();
                _this->tcdm_req->set_addr(_this->next_addr());
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
                _this->tcdm_req->init();
                _this->tcdm_req->set_addr(_this->next_addr());
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
                int modeled_runtime = _this->get_redmule_array_runtime();
                int64_t latency = 1;

                // Compensation
                if (_this->fsm_timestamp >= modeled_runtime)
                {
                    _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] TCDM Access Time(%d) > Model Time(%d)\n", _this->iter_i, _this->iter_j, _this->iter_k, _this->fsm_timestamp, modeled_runtime);
                    latency = 1;
                } else {
                    _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] Model Time(%d) > TCDM Access Time(%d)\n",  _this->iter_i, _this->iter_j, _this->iter_k, modeled_runtime, _this->fsm_timestamp);
                    latency = modeled_runtime - _this->fsm_timestamp + 1;
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
                    latency += _this->get_routine_to_storing_latency();
                }

                _this->event_enqueue(_this->fsm_event, latency);
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
                _this->tcdm_req->init();
                _this->tcdm_req->set_addr(_this->next_addr());
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
                _this->event_enqueue(_this->fsm_event, 1);

                //report
                int64_t start_time_ns   = (_this->timer_start)/1000;
                int64_t end_time_ns     = (_this->time.get_time())/1000;
                int64_t period_ns       = end_time_ns - start_time_ns;
                double  period_uti      = (1.0 * _this->ideal_runtime)/(1.0 * period_ns);
                int64_t gemm_size       = 2 * (_this->m_size * _this->n_size + _this->n_size * _this->k_size + 2 * _this->m_size * _this->k_size);
                double  redmul_eff      = period_uti * (1.0 * _this->ce_height * _this->ce_width) / (1.0 * gemm_size);
                _this->total_runtime   += period_ns;
                _this->num_matmul      += 1;
                _this->trace.msg("[LightRedmule] Finished : %0d ns ---> %0d ns | period = %0d ns | uti = %0.3f | runtime = %0d ns | matmul id = %0d\n", start_time_ns, end_time_ns, period_ns, period_uti, _this->total_runtime, _this->num_matmul);
                _this->trace.msg("[LightRedmule] Area : %d | Efficiency : %f \n", gemm_size*_this->elem_size ,redmul_eff);
            } else {
                _this->state.set(STORING);
                _this->event_enqueue(_this->fsm_event, 1);
            }
            break;

        case FINISHED:
            //Reply Stalled Query
            if (_this->redmule_query == NULL)
            {
                _this->state.set(FINISHED);
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][FINISHED] Waiting for query!\n");
            } else {
                _this->state.set(ACKNOWLEDGE);
            }
            _this->event_enqueue(_this->fsm_event, 1);
            break;

        case ACKNOWLEDGE:
            _this->redmule_query->get_resp_port()->resp(_this->redmule_query);
            _this->redmule_query = NULL;
            _this->state.set(IDLE);
            _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][ACKNOWLEDGE] Done!\n");
            break;

        default:
            _this->trace.fatal("[LightRedmule] INVALID RedMule Status: %d\n", _this->state);
    }
}


