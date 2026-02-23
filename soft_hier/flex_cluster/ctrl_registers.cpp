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
 * Authors: Germain Haugou, ETH Zurich (germain.haugou@iis.ee.ethz.ch)
            Yichao  Zhang , ETH Zurich (yiczhang@iis.ee.ethz.ch)
            Chi     Zhang , ETH Zurich (chizhang@iis.ee.ethz.ch)
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <unordered_map>
#include <set>


class CtrlRegisters : public vp::Component
{

public:
    CtrlRegisters(vp::ComponentConf &config);
    ~CtrlRegisters();

private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void debug_stop_event_handler(vp::Block *__this, vp::ClockEvent *event);
    static void hbm_preload_done_handler(vp::Block *__this, bool value);
    static void hbm_preload_done_to_cluster_handler(vp::Block *__this, vp::ClockEvent *event);
    void reset(bool active);

    vp::Trace trace;
    vp::IoSlave input_itf;
    vp::WireSlave<bool> hbm_preload_done_itf;
    vp::ClockEvent * debug_stop_event;
    vp::WireMaster<bool> * hbm_preload_done_to_cluster_itf_array;
    vp::ClockEvent * hbm_preload_done_to_cluster_event;
    int64_t timer_start;
    uint32_t num_cluster_x;
    uint32_t num_cluster_y;
    uint32_t num_cluster_all;
    uint32_t has_preload_binary;
    uint32_t hbm_preload_done;
};



CtrlRegisters::CtrlRegisters(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->num_cluster_x = this->get_js_config()->get("num_cluster_x")->get_int();
    this->num_cluster_y = this->get_js_config()->get("num_cluster_y")->get_int();
    this->has_preload_binary = this->get_js_config()->get("has_preload_binary")->get_int();
    this->num_cluster_all = this->num_cluster_x * this->num_cluster_y;

    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->input_itf.set_req_meth(&CtrlRegisters::req);
    this->hbm_preload_done_itf.set_sync_meth(&CtrlRegisters::hbm_preload_done_handler);

    this->new_slave_port("input", &this->input_itf);
    this->hbm_preload_done_to_cluster_itf_array = new vp::WireMaster<bool>[this->num_cluster_all];
    for (int i = 0; i < this->num_cluster_all; ++i)
    {
        this->new_master_port("hbm_preload_done_to_cluster_"  + std::to_string(i), &(this->hbm_preload_done_to_cluster_itf_array[i]));
    }
    this->new_slave_port("hbm_preload_done", &this->hbm_preload_done_itf);
    this->debug_stop_event = this->event_new(&CtrlRegisters::debug_stop_event_handler);
    this->hbm_preload_done_to_cluster_event = this->event_new(&CtrlRegisters::hbm_preload_done_to_cluster_handler);
    this->timer_start = 0;
    this->hbm_preload_done = (this->has_preload_binary == 0)? 1:0;
}

void CtrlRegisters::reset(bool active)
{
    if (active)
    {
        std::cout << "[SystemInfo]: num_cluster_x = " << this->num_cluster_x << ", num_cluster_y = " << this->num_cluster_y << std::endl;
        this->event_enqueue(this->hbm_preload_done_to_cluster_event, 300);
    }
}

void CtrlRegisters::debug_stop_event_handler(vp::Block *__this, vp::ClockEvent *event){
    CtrlRegisters *_this = (CtrlRegisters *)__this;
    std::cout << "[Debug Stop]: Stop simulation at cycle: " << _this->clock.get_cycles() << std::endl;
    _this->time.get_engine()->quit(1);
}

void CtrlRegisters::hbm_preload_done_to_cluster_handler(vp::Block *__this, vp::ClockEvent *event){
    CtrlRegisters *_this = (CtrlRegisters *)__this;
    if (_this->hbm_preload_done)
    {
        _this->trace.msg("HBM Preload Done, Acknowledge all Clusters\n");
        for (int i = 0; i < _this->num_cluster_all; ++i)
        {
            _this->hbm_preload_done_to_cluster_itf_array[i].sync(1);
        }
    } else {
        _this->event_enqueue(_this->hbm_preload_done_to_cluster_event, 300);
    }
}


void CtrlRegisters::hbm_preload_done_handler(vp::Block *__this, bool value)
{
    CtrlRegisters *_this = (CtrlRegisters *)__this;
    _this->hbm_preload_done = 1;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "HBM Preloading Done\n");
}


vp::IoReqStatus CtrlRegisters::req(vp::Block *__this, vp::IoReq *req)
{
    CtrlRegisters *_this = (CtrlRegisters *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    // _this->trace.msg("Control registers access (offset: 0x%x, size: 0x%x, is_write: %d, data:%x)\n", offset, size, is_write, *(uint32_t *)data);

    if (is_write && size == 4)
    {
        uint32_t value = *(uint32_t *)data;
        if (offset == 0)
        {
            // std::cout << "EOC register return value: 0x" << std::hex << value << std::endl;
            _this->time.get_engine()->quit(0);
        }
        if (offset == 4)
        {
            //Do nothing
        }
        if (offset == 8)
        {
            _this->timer_start = _this->time.get_time();
        }
        if (offset == 12)
        {
            int64_t period = _this->time.get_time() - _this->timer_start;
            std::cout << "[Performance Counter]: Execution period is " << period/1000 << " ns" << std::endl;
            _this->timer_start = _this->time.get_time();
        }
        if (offset == 16)
        {
            char c = (char)value;
            std::cout << c;
        }
        if (offset == 20)
        {
            std::cout << value;
        }
        if (offset == 40) {
            //Stop at Time
            _this->event_enqueue(_this->debug_stop_event, value);
        }
    }

    return vp::IO_REQ_OK;
}

CtrlRegisters::~CtrlRegisters(){
    delete[] this->hbm_preload_done_to_cluster_itf_array;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new CtrlRegisters(config);
}