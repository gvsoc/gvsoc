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
#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include "c2c_platform.hpp"


class Router : public vp::Component
{

public:
    Router(vp::ComponentConf &config);

private:
    static void routing_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req, int port);
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void response(vp::Block *__this, vp::IoReq *req);
    vp::IoReq * new_req(vp::IoReq *req, int port);
    vp::IoReq * del_req(vp::IoReq *req);
    int get_routing_port(vp::IoReq *req);
    int process_rx_port(int port);

    vp::Trace                               trace;
    std::vector<vp::IoSlave>                input_itf;
    std::vector<vp::IoMaster>               output_itf;
    vp::IoMaster                            top_ift;
    vp::ClockEvent *                        routing_fsm_event;

    int                                     radix;
    int                                     virtual_ch;
    int                                     rrb_port;
    std::vector<int>                        output_stall_list;
    std::vector<vp::IoReq *>                input_stall_req;
    std::vector<std::vector<vp::IoReq *>>   rx_fifo;
};


Router::Router(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->radix              = this->get_js_config()->get("radix")->get_int();
    this->virtual_ch              = this->get_js_config()->get("virtual_ch")->get_int();
    this->routing_fsm_event     = this->event_new(&Router::routing_fsm_handler);
    this->rrb_port              = 0;
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->input_itf.resize(this->radix);
    this->output_itf.resize(this->radix);
    this->rx_fifo.resize(this->radix);
    this->new_master_port("top_req", &this->top_ift);
    for (int i = 0; i < this->radix; ++i)
    {
        this->new_slave_port("in_" + std::to_string(i), &this->input_itf[i]);
        this->new_master_port("out_" + std::to_string(i), &this->output_itf[i]);
        this->output_itf[i].set_grant_meth(&Router::grant);
        this->output_itf[i].set_resp_meth(&Router::response);
        this->input_itf[i].set_req_meth_muxed(&Router::req, i);
        this->output_stall_list.push_back(0);
        this->input_stall_req.push_back(NULL);
    }
}

vp::IoReqStatus Router::req(vp::Block *__this, vp::IoReq *req, int port)
{
    Router *_this = (Router *)__this;
    
    //Check if RX FIFO is full
    if (_this->rx_fifo[port].size() >= _this->virtual_ch) {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "[Router] Port %d RX FIFO is full, reject the request\n", port);
        _this->input_stall_req[port] = req;
        return vp::IO_REQ_DENIED;
    } else {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "[Router] Port %d Accept the request, put it into RX FIFO\n", port);
        vp::IoReq * tmp_req = _this->new_req(req, port);
        _this->rx_fifo[port].push_back(tmp_req);
        _this->event_enqueue(_this->routing_fsm_event, 1);
        return vp::IO_REQ_OK;
    }
}

int Router::get_routing_port(vp::IoReq *req)
{
    int source_port = *(int *)req->arg_get(C2CPlatform::REQ_PORT_ID);
    vp::IoReq * tmp_req = this->new_req(req, source_port);
    vp::IoReqStatus result = this->top_ift.req(tmp_req);
    if (result != vp::IO_REQ_OK)
    {
        this->trace.fatal("[Router] Error: No valid topology manager\n");
    }
    int destination_port = *(int *)tmp_req->arg_get(C2CPlatform::REQ_PORT_ID);
    this->del_req(tmp_req);
    return destination_port;
}

int Router::process_rx_port(int port)
{
    int forwarded = 0;
    int num_req = this->rx_fifo[port].size();
    std::vector<vp::IoReq *> remain_req;
    for (int i = 0; i < num_req; ++i)
    {
        vp::IoReq * req = this->rx_fifo[port][i];

        //Get routing port
        int rid = this->get_routing_port(req);

        //Check if stalls
        if (this->output_stall_list[rid] == 0)
        {
            //Set routing port
            *req->arg_get(C2CPlatform::REQ_PORT_ID) = (void *)rid;

            //Forward req
            vp::IoReqStatus result = this->output_itf[rid].req(req);

            //Check results
            if (result == vp::IO_REQ_INVALID)
            {
                this->trace.fatal("[Router] Error: output port %d request failed\n", rid);
            } else {
                if (result == vp::IO_REQ_DENIED)
                {
                    if (this->output_stall_list[rid] == 0) {
                        this->output_stall_list[rid] = 1;
                        this->trace.msg(vp::Trace::LEVEL_TRACE, "[Router] Output port %d is stalled\n", rid);
                    } else {
                        this->trace.fatal("[Router] Error: Output port %d is still stalled but we are trying to send a new request\n", rid);
                    }
                }

                if (result == vp::IO_REQ_OK)
                {
                    this->trace.msg(vp::Trace::LEVEL_TRACE, "[Router] delete req %p\n", req);
                    this->del_req(req);
                }

                //Set forwarded
                forwarded = 1;
            }
        } else {
            remain_req.push_back(req);
        }
    }

    //Remove forwarded requests
    this->rx_fifo[port] = remain_req;

    //Check input stalls
    if (this->input_stall_req[port] != NULL && this->rx_fifo[port].size() < this->virtual_ch)
    {
        //Push to rx fifo
        vp::IoReq * tmp_req = this->new_req(this->input_stall_req[port], port);
        this->rx_fifo[port].push_back(tmp_req);

        //Response request
        this->input_stall_req[port]->get_resp_port()->grant(this->input_stall_req[port]);
        this->input_stall_req[port]->get_resp_port()->resp(this->input_stall_req[port]);
        this->input_stall_req[port] = NULL;
    }

    return forwarded;
}

void Router::routing_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Router *_this = (Router *)__this;

    //Iterate over ports
    int last_forward_port = _this->rrb_port;
    for (int i = 0; i < _this->radix; ++i)
    {
        int pid = (i + _this->rrb_port) % _this->radix;
        int res = _this->process_rx_port(pid);
        if (res)
        {
            last_forward_port = pid;
        }
    }
    _this->rrb_port = (last_forward_port + 1) % _this->radix;

    //Check if there still exist requests in rx fifos
    int num_rx_req = 0;
    for (auto x : _this->rx_fifo) {
        num_rx_req += x.size();
    }
    if (num_rx_req > 0) {
        _this->event_enqueue(_this->routing_fsm_event, 1);
    }
}

void Router::response(vp::Block *__this, vp::IoReq *req)
{
    Router *_this = (Router *)__this;
    _this->trace.msg(vp::Trace::LEVEL_TRACE, "[Router CallBack] Receive response from output interface\n");
    _this->trace.msg(vp::Trace::LEVEL_TRACE, "[Router CallBack] delete req %p\n", req);
    _this->del_req(req);
}

// This gets called after a request sent to a target was denied, and it is now granted
void Router::grant(vp::Block *__this, vp::IoReq *req)
{
    Router *_this = (Router *)__this;
    int i = *(int *)req->arg_get(C2CPlatform::REQ_PORT_ID);

    if (_this->output_stall_list[i] == 1) {
        _this->output_stall_list[i] = 0;
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Output port %d is un-stalled\n", i);
        _this->event_enqueue(_this->routing_fsm_event, 1);
    } else {
        _this->trace.fatal("[Router] Error: output port %d is not stalled but we are granted\n", i);
    }
}

vp::IoReq * Router::new_req(vp::IoReq *req, int port_id)
{
    vp::IoReq * tmp_req = new vp::IoReq();
    tmp_req->init();
    tmp_req->arg_alloc(C2CPlatform::REQ_NB_ARGS);
    for (int i = 0; i < C2CPlatform::REQ_NB_ARGS; ++i)
    {
        *tmp_req->arg_get(i) = *req->arg_get(i);
    }
    *tmp_req->arg_get(C2CPlatform::REQ_PORT_ID) = (void *)port_id;
    tmp_req->set_addr(req->get_addr());
    tmp_req->set_size(req->get_size());
    tmp_req->set_is_write(req->get_is_write());
    uint8_t * data = new uint8_t[req->get_size()];
    memcpy(data, req->get_data(), req->get_size());
    tmp_req->set_data(data);
    return tmp_req;
}

vp::IoReq * Router::del_req(vp::IoReq *req)
{
    uint8_t * data = (uint8_t *)req->get_data();
    delete[] data;
    delete req;
    return NULL;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Router(config);
}
