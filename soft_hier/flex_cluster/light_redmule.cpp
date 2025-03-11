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

typedef union {
    float f;
    struct {
        uint32_t mantissa : 23;
        uint32_t exponent : 8;
        uint32_t sign : 1;
    } parts;
} FloatBits;

typedef uint8_t  fp8e4m3;
typedef uint16_t fp16;

enum redmule_state {
    IDLE,
    PRELOAD,
    ROUTINE,
    STORING,
    FINISHED,
    ACKNOWLEDGE
};

enum iter_instruction {
    INSTR_LOAD_Y,
    INSTR_LOAD_W,
    INSTR_LOAD_W_COMPUTE,
    INSTR_LOAD_X,
    INSTR_STOR_Z,
    INSTR_FORWARD_YZ
};

/********************************************************
*                   Function Definition                 *
********************************************************/

void matmul_uint16(uint16_t * z, uint16_t * y, uint16_t * x, uint16_t * w, uint16_t m_size, uint16_t n_size, uint16_t k_size);
void matmul_int16(int16_t * z, int16_t * y, int16_t * x, int16_t * w, uint16_t m_size, uint16_t n_size, uint16_t k_size);
void matmul_fp16(fp16 * z, fp16 * y, fp16 * x, fp16 * w, uint16_t m_size, uint16_t n_size, uint16_t k_size);
void matmul_uint8(uint8_t * z, uint8_t * y, uint8_t * x, uint8_t * w, uint16_t m_size, uint16_t n_size, uint16_t k_size);
void matmul_int8(int8_t * z, int8_t * y, int8_t * x, int8_t * w, uint16_t m_size, uint16_t n_size, uint16_t k_size);
void matmul_fp8e4m3(fp8e4m3 * z, fp8e4m3 * y, fp8e4m3 * x, fp8e4m3 * w, uint16_t m_size, uint16_t n_size, uint16_t k_size);

/*****************************************************
*                   Class Definition                 *
*****************************************************/


class LightRedmule : public vp::Component
{

public:
    LightRedmule(vp::ComponentConf &config);

// private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static vp::IoReqStatus core_acc_req(vp::Block *__this, vp::IoReq *req);
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
    void     process_iter_instruction();
    void     process_compute();

    vp::Trace           trace;
    vp::IoSlave         input_itf;
    vp::IoSlave         core_acc_itf;
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
    int64_t             cycle_start;
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
    uint32_t            fold_tiles_mapping;
    uint32_t            compute_able;
    uint32_t            LOCAL_BUFFER_H;
    uint32_t            LOCAL_BUFFER_N;
    uint32_t            LOCAL_BUFFER_W;

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
    uint32_t            z_store_width;
    uint32_t            z_store_height;
    double              ideal_runtime;
    uint32_t            iter_instruction;
    uint32_t            iter_x_row_ptr;
    uint32_t            iter_w_row_ptr;
    uint32_t            iter_y_row_ptr;
    uint32_t            iter_z_row_ptr;

    //redmule buffer
    uint8_t *           access_buffer;
    uint8_t *           y_buffer_preload;
    uint8_t *           w_buffer;
    uint8_t *           x_buffer;
    uint8_t *           z_buffer_compute;
    uint8_t *           z_buffer_previos;
    
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
    this->core_acc_itf.set_req_meth(&LightRedmule::core_acc_req);
    this->new_slave_port("input", &this->input_itf);
    this->new_slave_port("core_acc", &this->core_acc_itf);
    this->new_master_port("tcdm", &this->tcdm_itf);
    
    
    //Initialize configuration
    this->tcdm_bank_width   = get_js_config()->get("tcdm_bank_width")->get_int();
    this->tcdm_bank_number  = get_js_config()->get("tcdm_bank_number")->get_int();
    this->elem_size         = get_js_config()->get("elem_size")->get_int();
    this->ce_height         = get_js_config()->get("ce_height")->get_int();
    this->ce_width          = get_js_config()->get("ce_width")->get_int();
    this->ce_pipe           = get_js_config()->get("ce_pipe")->get_int();
    this->queue_depth       = get_js_config()->get("queue_depth")->get_int();
    this->fold_tiles_mapping= get_js_config()->get("fold_tiles_mapping")->get_int();
    this->compute_able      = 0;
    this->bandwidth         = this->tcdm_bank_width * this->tcdm_bank_number;
    this->LOCAL_BUFFER_H    = this->ce_height;
    this->LOCAL_BUFFER_N    = this->bandwidth / this->elem_size;
    this->LOCAL_BUFFER_W    = this->ce_width * (this->ce_pipe + 1);

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
    this->z_store_width     = 0;
    this->z_store_height    = 0;
    this->ideal_runtime     = 0;
    this->iter_instruction  = INSTR_LOAD_Y;
    this->iter_x_row_ptr    = 0;
    this->iter_w_row_ptr    = 0;
    this->iter_y_row_ptr    = 0;
    this->iter_z_row_ptr    = 0;

    //Initialize Buffers
    this->access_buffer     = new uint8_t[this->bandwidth * 2];
    this->y_buffer_preload  = new uint8_t [this->LOCAL_BUFFER_H * this->LOCAL_BUFFER_W * this->elem_size];
    this->w_buffer          = new uint8_t [this->LOCAL_BUFFER_N * this->LOCAL_BUFFER_W * this->elem_size];
    this->x_buffer          = new uint8_t [this->LOCAL_BUFFER_H * this->LOCAL_BUFFER_N * this->elem_size];
    this->z_buffer_compute  = new uint8_t [this->LOCAL_BUFFER_H * this->LOCAL_BUFFER_W * this->elem_size];
    this->z_buffer_previos  = new uint8_t [this->LOCAL_BUFFER_H * this->LOCAL_BUFFER_W * this->elem_size];

    //Initialize FSM
    this->state.set(IDLE);
    this->redmule_query     = NULL;
    this->tcdm_req          = this->tcdm_itf.req_new(0, 0, 0, 0);
    this->fsm_event         = this->event_new(&LightRedmule::fsm_handler);
    this->tcdm_block_total  = 0;
    this->fsm_counter       = 0;
    this->fsm_timestamp     = 0;
    this->timer_start       = 0;
    this->cycle_start       = 0;
    this->total_runtime     = 0;
    this->num_matmul        = 0;

    this->trace.msg("[LightRedmule] Model Initialization Done!\n");
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

    this->iter_x_row_ptr = 0;
    this->iter_w_row_ptr = 0;
    this->iter_y_row_ptr = 0;
    this->iter_z_row_ptr = 0;

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
        this->tcdm_req->set_is_write(0);
        this->tcdm_req->set_size(buffer_w * this->elem_size);
        this->iter_instruction  = INSTR_LOAD_Y;
    } else if (this->w_acc_block > 0)
    {
        addr = this->iter_w_addr;
        this->iter_w_addr = this->inc_addr(addr, this->k_size, buffer_w);
        this->w_acc_block -= 1;
        this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Address] W tile at 0x%11x | #W tile left %d\n", addr, this->w_acc_block);
        this->tcdm_req->set_is_write(0);
        this->tcdm_req->set_size(buffer_w * this->elem_size);
        if (this->w_acc_block == 0)
        {
            this->iter_instruction  = INSTR_LOAD_W_COMPUTE;
        } else {
            this->iter_instruction  = INSTR_LOAD_W;
        }
    } else if (this->x_acc_block > 0)
    {
        addr = this->iter_x_addr;
        this->iter_x_addr = this->inc_addr(addr, this->n_size, buffer_n);
        this->x_acc_block -= 1;
        this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Address] X tile at 0x%11x | #X tile left %d\n", addr, this->x_acc_block);
        this->tcdm_req->set_is_write(0);
        this->tcdm_req->set_size(this->bandwidth);
        this->iter_instruction  = INSTR_LOAD_X;
    } else if (this->z_acc_block > 0)
    {
        addr = this->iter_z_addr;
        this->iter_z_addr = this->inc_addr(addr, this->k_size, buffer_w);
        this->z_acc_block -= 1;
        this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Address] Z tile at 0x%11x | #Z tile left %d\n", addr, this->z_acc_block);
        this->tcdm_req->set_is_write(1);
        this->tcdm_req->set_size(this->z_store_width * this->elem_size);
        this->iter_instruction  = INSTR_STOR_Z;
    } else {
        this->trace.fatal("[LightRedmule][Address] INVALID redmule address iteration : No tiles to access\n");
    }

    return addr;
}

void LightRedmule::process_compute(){
    uint32_t buffer_h           = this->ce_height;
    uint32_t buffer_w           = this->ce_width * (this->ce_pipe + 1);
    uint32_t buffer_n           = this->bandwidth / this->elem_size;

    if (this->compute_able == 1)
    {
        //UINT16
        matmul_uint16(  (uint16_t *)this->z_buffer_compute,
                        (uint16_t *)this->z_buffer_compute,
                        (uint16_t *)this->x_buffer,
                        (uint16_t *)this->w_buffer,
                        (uint16_t)buffer_h,
                        (uint16_t)buffer_n,
                        (uint16_t)buffer_w);
    } else
    if (this->compute_able == 2)
    {
        //INT16
        matmul_int16(   (int16_t *)this->z_buffer_compute,
                        (int16_t *)this->z_buffer_compute,
                        (int16_t *)this->x_buffer,
                        (int16_t *)this->w_buffer,
                        (uint16_t)buffer_h,
                        (uint16_t)buffer_n,
                        (uint16_t)buffer_w);
    } else
    if (this->compute_able == 3)
    {
        //FP16
        matmul_fp16(    (fp16 *)this->z_buffer_compute,
                        (fp16 *)this->z_buffer_compute,
                        (fp16 *)this->x_buffer,
                        (fp16 *)this->w_buffer,
                        (uint16_t)buffer_h,
                        (uint16_t)buffer_n,
                        (uint16_t)buffer_w);
    }
    if (this->compute_able == 5)
    {
        //UINT8
        matmul_uint8(   (uint8_t *)this->z_buffer_compute,
                        (uint8_t *)this->z_buffer_compute,
                        (uint8_t *)this->x_buffer,
                        (uint8_t *)this->w_buffer,
                        (uint16_t)buffer_h,
                        (uint16_t)buffer_n,
                        (uint16_t)buffer_w);
    }
    if (this->compute_able == 6)
    {
        //UINT8
        matmul_int8(    (int8_t *)this->z_buffer_compute,
                        (int8_t *)this->z_buffer_compute,
                        (int8_t *)this->x_buffer,
                        (int8_t *)this->w_buffer,
                        (uint16_t)buffer_h,
                        (uint16_t)buffer_n,
                        (uint16_t)buffer_w);
    }
    if (this->compute_able == 7)
    {
        //UINT8
        matmul_fp8e4m3(     (fp8e4m3 *)this->z_buffer_compute,
                        (fp8e4m3 *)this->z_buffer_compute,
                        (fp8e4m3 *)this->x_buffer,
                        (fp8e4m3 *)this->w_buffer,
                        (uint16_t)buffer_h,
                        (uint16_t)buffer_n,
                        (uint16_t)buffer_w);
    }

}

void LightRedmule::process_iter_instruction(){

    /*
                      buffer_w, 
                      k_size, 
                      iter_j
            buffer_n|------
            n_size  |  W  |
            iter_k  |     |
    buffer_h|--------------
    m_size  |  X    |  Y/Z|
    iter_i  |-------|------
    */

    uint32_t buffer_h_byte = this->ce_height * this->elem_size;
    uint32_t buffer_n_byte = this->bandwidth;
    uint32_t buffer_w_byte = this->ce_width * (this->ce_pipe + 1) * this->elem_size;

    uint32_t _k = this->iter_k + 1;
    if ((_k == this->x_row_tiles) || this->state.get() == PRELOAD)
    {
        _k = 0;
    }
    uint32_t x_leftover_byte = this->n_size * this->elem_size - _k * buffer_n_byte;
    uint32_t x_cutoff = x_leftover_byte < buffer_n_byte ? x_leftover_byte : buffer_n_byte;

    uint32_t w_leftover_byte = this->k_size * this->elem_size - this->iter_j * buffer_w_byte;
    uint32_t w_cutoff = w_leftover_byte < buffer_w_byte ? w_leftover_byte : buffer_w_byte;

    uint32_t _j = this->iter_j + 1;
    if ((_j == this->z_row_tiles) || this->state.get() == PRELOAD)
    {
        _j = 0;
    }
    uint32_t y_leftover_byte = this->k_size * this->elem_size - _j * buffer_w_byte;
    uint32_t y_cutoff = y_leftover_byte < buffer_w_byte ? y_leftover_byte : buffer_w_byte;

    uint32_t z_width_leftover_byte = this->k_size * this->elem_size - this->iter_j * buffer_w_byte;
    uint32_t z_width_cutoff = z_width_leftover_byte < buffer_w_byte ? z_width_leftover_byte : buffer_w_byte;
    uint32_t z_height_leftover_byte = this->m_size * this->elem_size - this->iter_i * buffer_h_byte;
    uint32_t z_height_cutoff = z_height_leftover_byte < buffer_h_byte ? z_height_leftover_byte : buffer_h_byte;

    uint32_t buffer_yz_byte = this->ce_height * this->ce_width * (this->ce_pipe + 1) * this->elem_size;
    switch(this->iter_instruction) {
        case INSTR_LOAD_Y:
            std::memcpy(&(this->y_buffer_preload[this->iter_y_row_ptr]), this->access_buffer, y_cutoff);
            if (this->y_acc_block == 0)
            {
                this->iter_y_row_ptr = 0;
            } else {
                this->iter_y_row_ptr += buffer_w_byte;
            }
            break;
        case INSTR_LOAD_W:
            std::memcpy(&(this->w_buffer[this->iter_w_row_ptr]), this->access_buffer, w_cutoff);
            this->iter_w_row_ptr += buffer_w_byte;
            break;
        case INSTR_LOAD_W_COMPUTE:
            std::memcpy(&(this->w_buffer[this->iter_w_row_ptr]), this->access_buffer, w_cutoff);
            this->process_compute();
            this->iter_w_row_ptr = 0;
            //Clear X and W buffer
            std::memset(this->x_buffer, 0, this->ce_height * this->bandwidth);
            std::memset(this->w_buffer, 0, this->ce_width * (this->ce_pipe + 1) * this->bandwidth);
            break;
        case INSTR_LOAD_X:
            std::memcpy(&(this->x_buffer[this->iter_x_row_ptr]), this->access_buffer, x_cutoff);
            if (this->x_acc_block == 0)
            {
                this->iter_x_row_ptr = 0;
            } else {
                this->iter_x_row_ptr += buffer_n_byte;
            }
            break;
        case INSTR_STOR_Z:
            std::memcpy(this->access_buffer, &(this->z_buffer_previos[this->iter_z_row_ptr]), buffer_w_byte);
            if (this->z_acc_block == 0)
            {
                this->iter_z_row_ptr = 0;
            } else {
                this->iter_z_row_ptr += buffer_w_byte;
            }
            break;
        case INSTR_FORWARD_YZ:
            std::memcpy(this->z_buffer_previos, this->z_buffer_compute, buffer_yz_byte);
            std::memcpy(this->z_buffer_compute, this->y_buffer_preload, buffer_yz_byte);
            std::memset(this->y_buffer_preload, 0, buffer_yz_byte);
            this->z_store_width = z_width_cutoff / this->elem_size;
            this->z_store_height = z_height_cutoff / this->elem_size;
            break;
        default:
            break;
    }
}

uint32_t LightRedmule::get_routine_access_block_number(){
    uint32_t total_blocks       = 0;
    uint32_t is_last_iteration  = (this->iter_i == (this->z_col_tiles - 1)) && (this->iter_j == (this->z_row_tiles - 1)) && (this->iter_k == (this->x_row_tiles - 1));
    uint32_t is_first_iteration = (this->iter_i == 0) && (this->iter_j == 0) && (this->iter_k == 0);
    uint32_t buffer_h           = this->ce_height;
    uint32_t buffer_w           = this->ce_width * (this->ce_pipe + 1);
    uint32_t buffer_n           = this->bandwidth / this->elem_size;
    uint32_t tcdms_bw           = buffer_n;

    this->x_acc_block = 0;
    this->w_acc_block = 0;
    this->y_acc_block = 0;
    this->z_acc_block = 0;
    

    // W block
    if (this->iter_k == (this->x_row_tiles - 1) && (this->x_row_lefts > 0))
    {
        this->w_acc_block = this->x_row_lefts;
    } else {
        this->w_acc_block = tcdms_bw;
    }
    total_blocks += this->w_acc_block;
    this->iter_w_addr = this->calculate_tile_base_address(this->w_addr, this->k_size, buffer_n, buffer_w, this->iter_k, this->iter_j);

    // X block
    if (is_last_iteration == 0)
    {

        //update x tile base address
        uint32_t _k = this->iter_k;
        uint32_t _j = this->iter_j;
        uint32_t _i = this->iter_i;
        _k += 1;
        if (_k == this->x_row_tiles)
        {
            _k = 0;
            _j += 1;
            if (_j == this->z_row_tiles)
            {
                _j = 0;
                _i += 1;
            }
        }
        this->iter_x_addr = this->calculate_tile_base_address(this->x_addr, this->n_size, buffer_h, buffer_n, _i, _k);

        this->x_acc_block = (this->m_size - _i * this->ce_height) < this->ce_height ? (this->m_size - _i * this->ce_height) : this->ce_height;
        total_blocks += this->x_acc_block;
    }

    // Y block
    if (this->iter_k == (this->x_row_tiles - 1) && (is_last_iteration == 0))
    {
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

        this->y_acc_block = (this->m_size - _i * this->ce_height) < this->ce_height ? (this->m_size - _i * this->ce_height) : this->ce_height;
        total_blocks += this->y_acc_block;
    }

    // Z block
    if ((this->iter_k == 0) && (is_first_iteration == 0))
    {
        this->z_acc_block = this->z_store_height;
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

    this->x_acc_block = 0;
    this->w_acc_block = 0;
    this->y_acc_block = 0;
    this->z_acc_block = 0;

    // X & Y block
    this->x_acc_block = this->m_size < this->ce_height ?  this->m_size : this->ce_height;
    this->y_acc_block = this->m_size < this->ce_height ?  this->m_size : this->ce_height;
    total_blocks = this->x_acc_block + this->y_acc_block;

    return total_blocks;

}

uint32_t LightRedmule::get_storing_access_block_number(){
    uint32_t total_blocks       = 0;
    uint32_t buffer_h           = this->ce_height;
    uint32_t buffer_w           = this->ce_width * (this->ce_pipe + 1);
    uint32_t buffer_n           = this->bandwidth / this->elem_size;
    uint32_t tcdms_bw           = this->bandwidth / this->elem_size;

    this->x_acc_block = 0;
    this->w_acc_block = 0;
    this->y_acc_block = 0;
    this->z_acc_block = 0;

    // Z block
    this->z_acc_block = this->z_store_height;
    total_blocks = this->z_acc_block;
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

vp::IoReqStatus LightRedmule::core_acc_req(vp::Block *__this, vp::IoReq *req)
{
    LightRedmule *_this = (LightRedmule *)__this;

    uint64_t offset = req->get_addr();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    if (offset == 0) //MNK configuraion
    {
        if (_this->state.get() == IDLE)
        {
            uint16_t * mnk_list_array = (uint16_t *)req->get_data();
            _this->m_size = mnk_list_array[0];
            _this->n_size = mnk_list_array[1];
            _this->k_size = mnk_list_array[2];
            _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set MNK addr: %d, %d, %d)\n", _this->m_size, _this->n_size, _this->k_size);
        }
    } else
    if (offset == 4) //XWY configuration and trigger
    {
        if (_this->state.get() == IDLE)
        {
             uint32_t * xwy_list_array = (uint32_t *)req->get_data();
            _this->x_addr = xwy_list_array[0];
            _this->w_addr = xwy_list_array[1];
            _this->y_addr = xwy_list_array[2];
            _this->z_addr = _this->y_addr;
            _this->compute_able = xwy_list_array[3];
            _this->elem_size = (_this->compute_able < 4)? 2:1;
            _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set XWY addr: %d, %d, %d)\n", _this->x_addr, _this->w_addr, _this->y_addr);

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
            _this->cycle_start      = _this->clock.get_cycles();
            _this->event_enqueue(_this->fsm_event, 1);
        }

    }

    return vp::IO_REQ_OK;
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
        _this->cycle_start      = _this->clock.get_cycles();
        _this->compute_able     = 0;
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
        _this->cycle_start      = _this->clock.get_cycles();
        _this->compute_able     = 0;
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
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set M size 0x%x)\n", value);
                break;
            case 4:
                _this->n_size = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set N size 0x%x)\n", value);
                break;
            case 8:
                _this->k_size = value;
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] Set K size 0x%x)\n", value);
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
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] write status\n");
                break;
            default:
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule] write to INVALID address\n");
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

                //Send request
                vp::IoReqStatus err = _this->send_tcdm_req();
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][Preload] --- Send TCDM req #%d\n",_this->fsm_counter);

                //Check error
                if (err != vp::IO_REQ_OK) {
                    _this->trace.fatal("[LightRedmule][Preload] There was an error while reading/writing data\n");
                    return;
                }

                //Process Data if Compute Enabled
                if (_this->compute_able != 0)
                {
                    _this->process_iter_instruction();
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

                //Forward YZ at Preload->Routine if Compute Enabled
                if (_this->compute_able != 0)
                {
                    _this->iter_instruction = INSTR_FORWARD_YZ;
                    _this->process_iter_instruction();
                }
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

                //Process Data if Compute Enabled
                if (_this->compute_able != 0 && (_this->iter_instruction == INSTR_STOR_Z))
                {
                    _this->process_iter_instruction();
                }

                //Send request
                vp::IoReqStatus err = _this->send_tcdm_req();
                _this->trace.msg(vp::Trace::LEVEL_TRACE,"[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] --- Send TCDM req #%d\n", _this->iter_i, _this->iter_j, _this->iter_k, _this->fsm_counter);

                //Check error
                if (err != vp::IO_REQ_OK) {
                    _this->trace.fatal("[LightRedmule][ROUTINE-ijk: %0d-%0d-%0d] There was an error while reading/writing data\n", _this->iter_i, _this->iter_j, _this->iter_k);
                    return;
                }

                //Process Data if Compute Enabled
                if (_this->compute_able != 0 && _this->iter_instruction != INSTR_STOR_Z)
                {
                    _this->process_iter_instruction();
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

                // Need Forward YZ
                if ((_this->compute_able != 0) && (_this->iter_k + 1 == _this->x_row_tiles))
                {
                    _this->iter_instruction = INSTR_FORWARD_YZ;
                    _this->process_iter_instruction();
                }

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

                //Process Data if Compute Enabled
                if (_this->compute_able != 0 && (_this->iter_instruction == INSTR_STOR_Z))
                {
                    _this->process_iter_instruction();
                }

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
                int64_t period_clk      = _this->clock.get_cycles() - _this->cycle_start;
                double  period_uti      = (1.0 * _this->ideal_runtime)/(1.0 * period_clk);
                int64_t gemm_size       = 2 * (_this->m_size * _this->n_size + _this->n_size * _this->k_size + 2 * _this->m_size * _this->k_size);
                double  redmul_eff      = period_uti * (1.0 * _this->ce_height * _this->ce_width) / (1.0 * gemm_size);
                _this->total_runtime   += period_ns;
                _this->num_matmul      += 1;
                _this->trace.msg("[LightRedmule] Finished : %0d ns ---> %0d ns | period = %0d ns (%0d cyc) | uti = %0.3f | runtime = %0d ns | GEMM id = %0d | FMT = %0d | M-N-K = %0d-%0d-%0d | X-W-Y = 0x%5x-0x%5x-0x%5x\n",
                    start_time_ns, end_time_ns, period_ns, period_clk, period_uti, _this->total_runtime, _this->num_matmul, _this->compute_able, _this->m_size, _this->n_size, _this->k_size, _this->x_addr, _this->w_addr, _this->y_addr);
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



/************************************************************
*                   Function Implementation                 *
************************************************************/

void matmul_uint16(uint16_t * z, uint16_t * y, uint16_t * x, uint16_t * w, uint16_t m_size, uint16_t n_size, uint16_t k_size){
    for (int i = 0; i < m_size; ++i)
    {
        for (int j = 0; j < k_size; ++j)
        {
            z[i * k_size + j] = y[i * k_size + j];
            for (int k = 0; k < n_size; ++k)
            {
                z[i * k_size + j] += x[i * n_size + k] * w[k * k_size + j];
            }
        }
    }
}

void matmul_int16(int16_t * z, int16_t * y, int16_t * x, int16_t * w, uint16_t m_size, uint16_t n_size, uint16_t k_size){
    for (int i = 0; i < m_size; ++i)
    {
        for (int j = 0; j < k_size; ++j)
        {
            z[i * k_size + j] = y[i * k_size + j];
            for (int k = 0; k < n_size; ++k)
            {
                z[i * k_size + j] += x[i * n_size + k] * w[k * k_size + j];
            }
        }
    }
}


// Convert float to FP16 (half-precision)
fp16 float_to_fp16(float value) {
    FloatBits floatBits;
    floatBits.f = value;

    uint16_t sign = floatBits.parts.sign << 15;
    int32_t exponent = floatBits.parts.exponent - 127 + 15; // adjust bias from 127 to 15
    uint32_t mantissa = floatBits.parts.mantissa >> 13;     // reduce to 10 bits

    if (exponent <= 0) {
        if (exponent < -10) return sign;   // too small
        mantissa = (floatBits.parts.mantissa | 0x800000) >> (1 - exponent);
        return sign | mantissa;
    } else if (exponent >= 0x1F) {
        return sign | 0x7C00;  // overflow to infinity
    }
    return sign | (exponent << 10) | mantissa;
}

// Convert FP16 to float
float fp16_to_float(fp16 value) {
    FloatBits floatBits;
    floatBits.parts.sign = (value >> 15) & 0x1;
    int32_t exponent = (value >> 10) & 0x1F;
    floatBits.parts.exponent = (exponent == 0) ? 0 : exponent + 127 - 15;
    floatBits.parts.mantissa = (value & 0x3FF) << 13;
    return floatBits.f;
}

// Fused multiply-add for FP16
fp16 fp16_fma(fp16 a, fp16 b, fp16 c) {
    float fa = fp16_to_float(a);
    float fb = fp16_to_float(b);
    float fc = fp16_to_float(c);
    float result = (fa * fb) + fc;
    return float_to_fp16(result);
}

void matmul_fp16(fp16 * z, fp16 * y, fp16 * x, fp16 * w, uint16_t m_size, uint16_t n_size, uint16_t k_size){
    for (int i = 0; i < m_size; ++i)
    {
        for (int j = 0; j < k_size; ++j)
        {
            z[i * k_size + j] = y[i * k_size + j];
            for (int k = 0; k < n_size; ++k)
            {
                z[i * k_size + j] = fp16_fma(x[i * n_size + k], w[k * k_size + j], z[i * k_size + j]);
            }
        }
    }
}


void matmul_uint8(uint8_t * z, uint8_t * y, uint8_t * x, uint8_t * w, uint16_t m_size, uint16_t n_size, uint16_t k_size){
    for (int i = 0; i < m_size; ++i)
    {
        for (int j = 0; j < k_size; ++j)
        {
            z[i * k_size + j] = y[i * k_size + j];
            for (int k = 0; k < n_size; ++k)
            {
                z[i * k_size + j] += x[i * n_size + k] * w[k * k_size + j];
            }
        }
    }
}

void matmul_int8(int8_t * z, int8_t * y, int8_t * x, int8_t * w, uint16_t m_size, uint16_t n_size, uint16_t k_size){
    for (int i = 0; i < m_size; ++i)
    {
        for (int j = 0; j < k_size; ++j)
        {
            z[i * k_size + j] = y[i * k_size + j];
            for (int k = 0; k < n_size; ++k)
            {
                z[i * k_size + j] += x[i * n_size + k] * w[k * k_size + j];
            }
        }
    }
}

// Constants for FP8-E4M3 format
#define FP8_EXP_MASK  0x78  // 0111 1000
#define FP8_FRAC_MASK 0x07  // 0000 0111
#define FP8_SIGN_MASK 0x80  // 1000 0000
#define FP8_BIAS      7

// Convert FP8 (E4M3) to float
float fp8e4m3_to_float(fp8e4m3 value) {
    uint8_t sign = (value & FP8_SIGN_MASK) >> 7;
    uint8_t exponent = (value & FP8_EXP_MASK) >> 3;
    uint8_t fraction = value & FP8_FRAC_MASK;

    if (exponent == 0) {
        // Subnormal number
        if (fraction == 0) return sign ? -0.0f : 0.0f;
        return (sign ? -1.0f : 1.0f) * (fraction / 8.0f) * powf(2, -6);
    } else if (exponent == 15) {
        // Infinity or NaN
        return fraction ? NAN : (sign ? -INFINITY : INFINITY);
    }

    // Normalized number
    float mantissa = 1.0f + (fraction / 8.0f);
    float result = mantissa * powf(2, exponent - FP8_BIAS);
    return sign ? -result : result;
}

// Convert float to FP8 (E4M3)
fp8e4m3 float_to_fp8e4m3(float value) {
    if (isnan(value)) return 0x7F;  // NaN representation
    if (isinf(value)) return value < 0 ? 0xF8 : 0x78;  // +/-Inf representation

    uint8_t sign = (value < 0) ? 0x80 : 0x00;
    value = fabsf(value);

    int exponent;
    float mantissa = frexpf(value, &exponent);

    if (value == 0.0f) return 0;  // Zero representation

    exponent += FP8_BIAS - 1;

    if (exponent < 1) {
        // Subnormal handling
        int frac = (int)roundf(value / powf(2, -6) * 8.0f);
        return sign | (frac & FP8_FRAC_MASK);
    } else if (exponent > 14) {
        // Clamp to infinity
        return sign | 0x78;
    }

    // Normal number
    uint8_t frac = (uint8_t)roundf((mantissa - 0.5f) * 16.0f);
    return sign | ((exponent << 3) & FP8_EXP_MASK) | (frac & FP8_FRAC_MASK);
}

// Fused Multiply-Add for FP8
fp8e4m3 fp8e4m3_fma(fp8e4m3 a, fp8e4m3 b, fp8e4m3 c) {
    float fa = fp8e4m3_to_float(a);
    float fb = fp8e4m3_to_float(b);
    float fc = fp8e4m3_to_float(c);

    float result = fa * fb + fc;
    return float_to_fp8e4m3(result);
}

void matmul_fp8e4m3(fp8e4m3 * z, fp8e4m3 * y, fp8e4m3 * x, fp8e4m3 * w, uint16_t m_size, uint16_t n_size, uint16_t k_size){
    for (int i = 0; i < m_size; ++i)
    {
        for (int j = 0; j < k_size; ++j)
        {
            z[i * k_size + j] = y[i * k_size + j];
            for (int k = 0; k < n_size; ++k)
            {
                z[i * k_size + j] = fp8e4m3_fma(x[i * n_size + k], w[k * k_size + j], z[i * k_size + j]);
            }
        }
    }
}