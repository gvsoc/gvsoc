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
#include "c2c_platform.hpp"

/*
* req -> [tx_fifo] -> [dl_fifo] -> [rx_fifo] -> output_itf
*              <----- [cr_fifo] <----------
*   <--req-->|<--tx_fsm-->|<--rx_fsm-->|<--out-->
*             handel stall
*/


class D2DLink : public vp::Component
{

public:
    D2DLink(vp::ComponentConf &config);

private:
    static void rx_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    static void tx_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    static void ot_fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void response(vp::Block *__this, vp::IoReq *req);
    vp::IoReq * new_req(vp::IoReq *req);
    vp::IoReq * del_req(vp::IoReq *req);

    typedef struct {
        vp::IoReq *req;
        int64_t delay_timestamp;
    } d2dlink_delay_flit_t;

    typedef struct {
        int allowed;
        int64_t delay_timestamp;
    } d2dlink_delay_credit_t;

    vp::Trace trace;
    vp::IoSlave input_itf;
    vp::IoMaster output_itf;
    vp::ClockEvent *rx_fsm_event;
    vp::ClockEvent *tx_fsm_event;
    vp::ClockEvent *ot_fsm_event;

    int link_id;
    int fifo_depth_rx;
    int fifo_depth_tx;
    int fifo_credit_bar;
    int fifo_credit_cnt;
    int flit_granularity_byte;
    int link_latency_ns;
    int link_bandwidth_GBps;
    int tx_allowed;
    int64_t next_tx_timestamp;
    int64_t bw_interval_ps;
    int output_stalled;

    std::queue<vp::IoReq *> rx_fifo;
    std::queue<vp::IoReq *> tx_fifo;
    std::queue<d2dlink_delay_flit_t> dl_fifo;
    std::queue<d2dlink_delay_credit_t> cr_fifo;
    vp::IoReq * stalled_tx_req = NULL;
};


D2DLink::D2DLink(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->input_itf.set_req_meth(&D2DLink::req);
    this->output_itf.set_grant_meth(&D2DLink::grant);
    this->output_itf.set_resp_meth(&D2DLink::response);
    this->new_slave_port("data_in", &this->input_itf);
    this->new_master_port("data_out", &this->output_itf);
    this->rx_fsm_event          = this->event_new(&D2DLink::rx_fsm_handler);
    this->tx_fsm_event          = this->event_new(&D2DLink::tx_fsm_handler);
    this->ot_fsm_event          = this->event_new(&D2DLink::ot_fsm_handler);

    this->link_id               = this->get_js_config()->get("link_id")->get_int();
    this->fifo_depth_rx         = this->get_js_config()->get("fifo_depth_rx")->get_int();
    this->fifo_depth_tx         = this->get_js_config()->get("fifo_depth_tx")->get_int();
    this->fifo_credit_bar       = this->get_js_config()->get("fifo_credit_bar")->get_int();
    this->flit_granularity_byte = this->get_js_config()->get("flit_granularity_byte")->get_int();
    this->link_latency_ns       = this->get_js_config()->get("link_latency_ns")->get_int();
    this->link_bandwidth_GBps   = this->get_js_config()->get("link_bandwidth_GBps")->get_int();
    this->tx_allowed            = this->fifo_depth_rx; //initially assume remote side RX FIFO is empty
    this->fifo_credit_cnt       = 0;
    this->output_stalled        = 0;
    this->next_tx_timestamp     = 0;
    this->bw_interval_ps        = (int64_t)(1000.0 * (double)this->flit_granularity_byte / (double)this->link_bandwidth_GBps);
}

vp::IoReqStatus D2DLink::req(vp::Block *__this, vp::IoReq *req)
{
    D2DLink *_this = (D2DLink *)__this;

    //Sanity check
    if (req->get_size() != _this->flit_granularity_byte) {
        _this->trace.fatal("Error: request size (0x%x) is not flit_granularity_byte (0x%x)\n", req->get_size(), _this->flit_granularity_byte);
    }
    if (req->get_is_write() == false) {
        _this->trace.fatal("Error: read request is not supported\n");
    }
    if (_this->stalled_tx_req != NULL) {
        _this->trace.fatal("Error: stalled_tx_req is not NULL when a new request arrives\n");
    }

    //Check if TX FIFO is full
    if (_this->tx_fifo.size() >= _this->fifo_depth_tx) {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "TX FIFO is full, reject the request\n");
        _this->stalled_tx_req = req;
        return vp::IO_REQ_DENIED;
    } else {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Accept the request, put it into TX FIFO\n");
        vp::IoReq * tmp_req = _this->new_req(req);
        _this->tx_fifo.push(tmp_req);
        _this->event_enqueue(_this->tx_fsm_event, 1);
        return vp::IO_REQ_OK;
    }
}   


void D2DLink::tx_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    D2DLink *_this = (D2DLink *)__this;

    //Receive credit from remote side
    if (!_this->cr_fifo.empty()) {
        d2dlink_delay_credit_t cr = _this->cr_fifo.front();
        if (cr.delay_timestamp <= _this->time.get_time()) {
            _this->tx_allowed += cr.allowed;
            _this->cr_fifo.pop();
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Receive credit from remote side, tx_allowed=%d\n", _this->tx_allowed);
        }
    }

    //Try to send a flit if possible
    if (_this->tx_allowed > 0 && !_this->tx_fifo.empty() && _this->next_tx_timestamp <= _this->time.get_time()) {
        vp::IoReq *req = _this->tx_fifo.front();
        d2dlink_delay_flit_t dl_flit;
        dl_flit.req = req;
        dl_flit.delay_timestamp = _this->link_latency_ns * 1000 + _this->time.get_time();
        _this->tx_allowed -= 1;
        _this->dl_fifo.push(dl_flit);
        _this->tx_fifo.pop();
        _this->next_tx_timestamp = _this->time.get_time() + _this->bw_interval_ps;
        _this->event_enqueue(_this->rx_fsm_event, 1);
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Send a flit to output interface, remaining TX FIFO size: %d\n", _this->tx_fifo.size());
    }

    //Handle stalled request if any
    if (_this->stalled_tx_req != NULL && _this->tx_fifo.size() < _this->fifo_depth_tx) {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Handle stalled request, put it into TX FIFO\n");
        vp::IoReq * tmp_req = _this->new_req(_this->stalled_tx_req);
        _this->tx_fifo.push(tmp_req);
        _this->stalled_tx_req->get_resp_port()->grant(_this->stalled_tx_req);
        _this->stalled_tx_req->get_resp_port()->resp(_this->stalled_tx_req);
        _this->stalled_tx_req = NULL;
        _this->event_enqueue(_this->tx_fsm_event, 1);
    }

    //Re-try in next cycle if there are still flits in TX FIFO or credits in CR FIFO
    if (!_this->tx_fifo.empty() || !_this->cr_fifo.empty()) {
        _this->event_enqueue(_this->tx_fsm_event, 1);
    }
}

void D2DLink::rx_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    D2DLink *_this = (D2DLink *)__this;

    //Receive delayed flit from remote side
    if (!_this->dl_fifo.empty()) {
        d2dlink_delay_flit_t dl_flit = _this->dl_fifo.front();
        if (dl_flit.delay_timestamp <= _this->time.get_time()) {
            if (_this->rx_fifo.size() < _this->fifo_depth_rx) {
                _this->rx_fifo.push(dl_flit.req);
                _this->dl_fifo.pop();
                _this->event_enqueue(_this->ot_fsm_event, 1);
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Receive a flit from remote side, RX FIFO size: %d\n", _this->rx_fifo.size());
            } else {
                _this->trace.fatal("Error: Credit mechanisim wrong, RX FIFO is full, cannot receive the flit from remote side\n");
            }
        }
    }

    //Re-try in next cycle if there are still flits in DL FIFO
    if (!_this->dl_fifo.empty()) {
        _this->event_enqueue(_this->rx_fsm_event, 1);
    }
}

void D2DLink::ot_fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    D2DLink *_this = (D2DLink *)__this;

    //Deal with RX FIFO if not empty
    if (!_this->rx_fifo.empty() && _this->output_stalled == 0) {
        vp::IoReq *req = _this->rx_fifo.front();
        //Send to output interface
        vp::IoReqStatus result = _this->output_itf.req(req);

        if (result == vp::IO_REQ_INVALID) {
            _this->trace.fatal("Error: output interface request failed\n");
        } else {
            if (result == vp::IO_REQ_DENIED) {
                if (_this->output_stalled == 0) {
                    _this->output_stalled = 1;
                    _this->trace.msg("Output interface is stalled\n");
                } else {
                    _this->trace.fatal("Error: Output interface is still stalled but we are trying to send a new request\n");
                }
            }
            if(result == vp::IO_REQ_OK) {
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "[D2DLink OT FSM] delete req %p\n", req);
                _this->del_req(req);
            }
            _this->rx_fifo.pop();
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Send a flit to output interface, remaining RX FIFO size: %d\n", _this->rx_fifo.size());
            _this->fifo_credit_cnt += 1;
            if (_this->fifo_credit_cnt >= _this->fifo_credit_bar) {
                //Send credit back to remote side
                d2dlink_delay_credit_t cr;
                cr.allowed = _this->fifo_credit_cnt;
                cr.delay_timestamp = _this->link_latency_ns * 1000 + _this->time.get_time();
                _this->cr_fifo.push(cr);
                _this->event_enqueue(_this->tx_fsm_event, 1);
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Send %d credits back to remote side, remaining CR FIFO size: %d\n", _this->fifo_credit_cnt, _this->cr_fifo.size());
                _this->fifo_credit_cnt = 0;
            }
        }
    }

    //Re-try in next cycle if RX FIFO is not empty
    if (!_this->rx_fifo.empty()) {
        _this->event_enqueue(_this->ot_fsm_event, 1);
    }

}

void D2DLink::response(vp::Block *__this, vp::IoReq *req)
{
    D2DLink *_this = (D2DLink *)__this;
    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Receive response from output interface\n");
    _this->trace.msg(vp::Trace::LEVEL_TRACE, "[D2DLink CallBack] delete req %p\n", req);
    _this->del_req(req);
}

// This gets called after a request sent to a target was denied, and it is now granted
void D2DLink::grant(vp::Block *__this, vp::IoReq *req)
{
    D2DLink *_this = (D2DLink *)__this;

    if (_this->output_stalled == 1) {
        _this->output_stalled = 0;
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Output interface is un-stalled\n");
        _this->event_enqueue(_this->ot_fsm_event, 1);
    } else {
        _this->trace.fatal("Error: output interface is not stalled but we are granted\n");
    }
}

vp::IoReq * D2DLink::new_req(vp::IoReq *req)
{
    vp::IoReq * tmp_req = new vp::IoReq();
    tmp_req->init();
    tmp_req->arg_alloc(C2CPlatform::REQ_NB_ARGS);
    for (int i = 0; i < C2CPlatform::REQ_NB_ARGS; ++i)
    {
        *tmp_req->arg_get(i) = *req->arg_get(i);
    }
    tmp_req->set_addr(req->get_addr());
    tmp_req->set_size(req->get_size());
    tmp_req->set_is_write(req->get_is_write());
    uint8_t * data = new uint8_t[req->get_size()];
    memcpy(data, req->get_data(), req->get_size());
    tmp_req->set_data(data);
    return tmp_req;
}

vp::IoReq * D2DLink::del_req(vp::IoReq *req)
{
    uint8_t * data = (uint8_t *)req->get_data();
    delete[] data;
    delete req;
    return NULL;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new D2DLink(config);
}
