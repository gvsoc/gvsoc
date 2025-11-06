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
 * Authors: Chi Zhang, ETH Zurich (chizhang@iis.ee.ethz.ch)
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <queue>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include "c2c_platform.hpp"

class Endpoint : public vp::Component
{

public:
    Endpoint(vp::ComponentConf &config);

private:
    void reset(bool active);
    static void                 tx_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    static vp::IoReqStatus      req(vp::Block *__this, vp::IoReq *req);
    static void                 start_sync(vp::Block *__this, bool value);
    vp::IoReqStatus             dummy_recv(vp::IoReq *req);
    void                        dummy_send();
    void                        ack_send(int dest_id);
    static void                 grant(vp::Block *__this, vp::IoReq *req);
    static void                 response(vp::Block *__this, vp::IoReq *req);
    vp::IoReq *                 new_req(int sour_id, int dest_id, int is_ack, int is_last);
    vp::IoReq *                 del_req(vp::IoReq *req);

    vp::Trace                   trace;
    vp::IoSlave                 input_itf;
    vp::IoMaster                output_itf;
    vp::ClockEvent *            tx_fsm_event;
    vp::WireSlave<bool>         start_itf;
    vp::WireMaster<bool>        barrier_req_itf;

    int                         endpoint_id;
    int                         use_trace_file;
    int                         num_tx_flit;
    int                         cnt_tx_flit;
    int                         cnt_rx_flit;
    int                         flit_granularity_byte;
    int                         tx_stalled;
    int64_t                     start_timestamp;
    int64_t                     end_timestamp;
    std::queue<int>             ack_queue;
    // handle to trace file if needed
    std::ifstream *             trace_file_handle;
};

Endpoint::Endpoint(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->input_itf.set_req_meth(&Endpoint::req);
    this->start_itf.set_sync_meth(&Endpoint::start_sync);
    this->output_itf.set_grant_meth(&Endpoint::grant);
    this->output_itf.set_resp_meth(&Endpoint::response);
    this->new_slave_port("data_in", &this->input_itf);
    this->new_master_port("data_out", &this->output_itf);
    this->new_slave_port("start", &this->start_itf);
    this->new_master_port("barrier_req", &this->barrier_req_itf);
    this->tx_fsm_event          = this->event_new(&Endpoint::tx_fsm_handler);

    this->endpoint_id           = this->get_js_config()->get("endpoint_id")->get_int();
    this->use_trace_file        = this->get_js_config()->get("use_trace_file")->get_int();
    this->num_tx_flit           = this->get_js_config()->get("num_tx_flit")->get_int();
    this->flit_granularity_byte = this->get_js_config()->get("flit_granularity_byte")->get_int();
    this->trace_file_handle     = NULL;
    if (this->use_trace_file)
    {
        this->trace_file_handle = new std::ifstream(this->get_js_config()->get("trace_file")->get_str());
        if (!this->trace_file_handle->is_open())
        {
            this->trace.fatal("Error: cannot open trace file %s\n", this->get_js_config()->get("trace_file")->get_str().c_str());
        }
    }
    this->cnt_tx_flit           = 0;
    this->cnt_rx_flit           = 0;
    this->tx_stalled            = 0;
    this->start_timestamp       = 0;
    this->end_timestamp         = 0;
}

void Endpoint::reset(bool active)
{
    if (!active)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Reset done\n");
    }
}

void Endpoint::start_sync(vp::Block *__this, bool value)
{
    Endpoint *_this = (Endpoint *)__this;

    _this->trace.msg(vp::DEBUG, "Start sending flits\n");
    _this->start_timestamp = _this->time.get_time();
    _this->event_enqueue(_this->tx_fsm_event, 1);
}

vp::IoReqStatus Endpoint::dummy_recv(vp::IoReq *req)
{
    this->cnt_rx_flit++;
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Received flit %d\n", this->cnt_rx_flit);
    int REQ_SOUR_ID     = *(int *)req->arg_get(C2CPlatform::REQ_SOUR_ID);
    int REQ_DEST_ID     = *(int *)req->arg_get(C2CPlatform::REQ_DEST_ID);
    int REQ_IS_ACK      = *(int *)req->arg_get(C2CPlatform::REQ_IS_ACK);
    int REQ_IS_LAST     = *(int *)req->arg_get(C2CPlatform::REQ_IS_LAST);
    int REQ_SIZE        = *(int *)req->arg_get(C2CPlatform::REQ_SIZE);
    int REQ_WRITE       = *(int *)req->arg_get(C2CPlatform::REQ_WRITE);

    if(REQ_IS_LAST == 1)
    {
        this->ack_queue.push(REQ_SOUR_ID);
        this->event_enqueue(this->tx_fsm_event, 1);
    }

    if (REQ_IS_ACK == 1)
    {
        //Report BW
        this->end_timestamp = this->time.get_time();
        double total_time_ns = (double)(this->end_timestamp - this->start_timestamp) / 1000.0;
        double bandwidth_GBps = ((double)(this->cnt_tx_flit * this->flit_granularity_byte) / total_time_ns);
        this->trace.msg(vp::DEBUG, "Sent %d flits, total time %.2f ns, bandwidth %.2f GB/s\n",
            this->cnt_tx_flit, total_time_ns, bandwidth_GBps);
        this->barrier_req_itf.sync(1);
    }

    return vp::IO_REQ_OK;
}

vp::IoReqStatus Endpoint::req(vp::Block *__this, vp::IoReq *req)
{
    Endpoint *_this = (Endpoint *)__this;
    return _this->dummy_recv(req);
}

void Endpoint::dummy_send()
{
    int REQ_SOUR_ID     = this->endpoint_id;
    int REQ_DEST_ID     = this->endpoint_id;
    if (this->use_trace_file)
    {
        // Read dest_id from trace file
        std::string line;
        if (std::getline(*this->trace_file_handle, line))
        {
            std::istringstream iss(line);
            int dest_id;
            if (!(iss >> dest_id))
            {
                this->trace.fatal("Error: cannot read dest_id from trace file\n");
            }
            REQ_DEST_ID = dest_id;
        }
        else
        {
            this->trace.fatal("Error: cannot read from trace file\n");
        }
    }
    int REQ_IS_ACK      = 0;
    int REQ_IS_LAST     = 0;
    this->cnt_tx_flit++;

    // Check if last
    if (this->cnt_tx_flit >= this->num_tx_flit)
    {
        REQ_IS_LAST = 1;
    }

    // Generate request
    vp::IoReq *req = this->new_req(REQ_SOUR_ID, REQ_DEST_ID, REQ_IS_ACK, REQ_IS_LAST);
    vp::IoReqStatus result = this->output_itf.req(req);

    // Process results
    if (result == vp::IO_REQ_INVALID) {
        this->trace.fatal("Error: output interface request failed\n");
    } else {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Sent flit %d, to dest %d\n", this->cnt_tx_flit, REQ_DEST_ID);
        if (result == vp::IO_REQ_DENIED) {
            if (this->tx_stalled == 0) {
                this->tx_stalled = 1;
                this->trace.msg(vp::Trace::LEVEL_TRACE, "Output interface is stalled\n");
            } else {
                this->trace.fatal("Error: Output interface is still stalled but we are trying to send a new request\n");
            }
        }
        if(result == vp::IO_REQ_OK) {
            this->del_req(req);
        }
    }
}

void Endpoint::ack_send(int dest_id)
{
    int REQ_SOUR_ID     = this->endpoint_id;
    int REQ_DEST_ID     = dest_id;
    int REQ_IS_ACK      = 1;
    int REQ_IS_LAST     = 0;

    // Generate request
    vp::IoReq *req = this->new_req(REQ_SOUR_ID, REQ_DEST_ID, REQ_IS_ACK, REQ_IS_LAST);
    vp::IoReqStatus result = this->output_itf.req(req);

    // Process results
    if (result == vp::IO_REQ_INVALID) {
        this->trace.fatal("Error: output interface request failed\n");
    } else {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Sent Ack to %d\n", dest_id);
        if (result == vp::IO_REQ_DENIED) {
            if (this->tx_stalled == 0) {
                this->tx_stalled = 1;
                this->trace.msg(vp::Trace::LEVEL_TRACE, "Output interface is stalled\n");
            } else {
                this->trace.fatal("Error: Output interface is still stalled but we are trying to send a new request\n");
            }
        }
        if(result == vp::IO_REQ_OK) {
            this->del_req(req);
        }
    }
}

void Endpoint::tx_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Endpoint *_this = (Endpoint *)__this;

    if (!_this->tx_stalled)
    {
        if (_this->ack_queue.size() > 0)
        {
            // Send Ack
            _this->ack_send(_this->ack_queue.front());
            _this->ack_queue.pop();
        } else if (_this->cnt_tx_flit < _this->num_tx_flit)
        {
            // Send Data
            _this->dummy_send();
        }
    }

    if ((_this->cnt_tx_flit < _this->num_tx_flit) || (_this->ack_queue.size() > 0)) {
        _this->event_enqueue(_this->tx_fsm_event, 1);
    }
}

void Endpoint::grant(vp::Block *__this, vp::IoReq *req)
{
    Endpoint *_this = (Endpoint *)__this;
    if (_this->tx_stalled == 1) {
        _this->tx_stalled = 0;
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Output interface is unstalled\n");
        _this->event_enqueue(_this->tx_fsm_event, 1);
    } else {
        _this->trace.fatal("Error: Output interface is not stalled but we receive a grant\n");
    }
}

void Endpoint::response(vp::Block *__this, vp::IoReq *req)
{
    Endpoint *_this = (Endpoint *)__this;
    _this->trace.msg(vp::Trace::LEVEL_TRACE, "[Endpoint CallBack] delete req %p\n", req);
    _this->del_req(req);
}

vp::IoReq * Endpoint::new_req(int sour_id, int dest_id, int is_ack, int is_last)
{
    vp::IoReq * tmp_req = new vp::IoReq();
    tmp_req->init();
    tmp_req->arg_alloc(C2CPlatform::REQ_NB_ARGS);
    *tmp_req->arg_get(C2CPlatform::REQ_SOUR_ID) = (void *)sour_id;
    *tmp_req->arg_get(C2CPlatform::REQ_DEST_ID) = (void *)dest_id;
    *tmp_req->arg_get(C2CPlatform::REQ_IS_ACK)  = (void *)is_ack;
    *tmp_req->arg_get(C2CPlatform::REQ_IS_LAST) = (void *)is_last;
    *tmp_req->arg_get(C2CPlatform::REQ_SIZE)    = (void *)(this->flit_granularity_byte);
    *tmp_req->arg_get(C2CPlatform::REQ_WRITE)   = (void *)1;
    tmp_req->set_addr(0);
    tmp_req->set_size(this->flit_granularity_byte);
    tmp_req->set_is_write(1);
    uint8_t * data = new uint8_t[this->flit_granularity_byte];
    tmp_req->set_data(data);
    return tmp_req;
}

vp::IoReq * Endpoint::del_req(vp::IoReq *req)
{
    uint8_t * data = (uint8_t *)req->get_data();
    delete[] data;
    delete req;
    return NULL;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Endpoint(config);
}

