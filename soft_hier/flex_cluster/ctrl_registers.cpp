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
    static void wakeup_event_handler(vp::Block *__this, vp::ClockEvent *event);
    static void debug_stop_event_handler(vp::Block *__this, vp::ClockEvent *event);
    static void hbm_preload_done_handler(vp::Block *__this, bool value);
    void reset(bool active);

    typedef struct {
        bool is_global;
        int group_id;
        uint64_t timestamp;
    } wakeup_payload_t;

    vp::Trace trace;
    vp::IoSlave input_itf;
    vp::WireMaster<bool> * barrier_ack_itf_array;
    vp::WireSlave<bool> hbm_preload_done_itf;
    vp::ClockEvent * wakeup_event;
    vp::ClockEvent * debug_stop_event;
    int64_t timer_start;
    uint32_t num_cluster_x;
    uint32_t num_cluster_y;
    uint32_t num_cluster_all;
    uint32_t has_preload_binary;
    uint32_t hbm_preload_done;

    std::queue<wakeup_payload_t>  wakeup_timestamp_queue;
    std::unordered_map<uint16_t, std::set<uint16_t>> sync_group_mapper;
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
    this->barrier_ack_itf_array = new vp::WireMaster<bool>[this->num_cluster_all];
    for (int i = 0; i < this->num_cluster_all; ++i)
    {
        this->new_master_port("barrier_ack_" + std::to_string(i), &(this->barrier_ack_itf_array[i]));
    }
    this->new_slave_port("hbm_preload_done", &this->hbm_preload_done_itf);
    this->wakeup_event = this->event_new(&CtrlRegisters::wakeup_event_handler);
    this->debug_stop_event = this->event_new(&CtrlRegisters::debug_stop_event_handler);
    this->timer_start = 0;
    this->hbm_preload_done = (this->has_preload_binary == 0)? 1:0;
}

void CtrlRegisters::reset(bool active)
{
    if (active)
    {
        std::cout << "[SystemInfo]: num_cluster_x = " << this->num_cluster_x << ", num_cluster_y = " << this->num_cluster_y << std::endl;
    }
}

void CtrlRegisters::debug_stop_event_handler(vp::Block *__this, vp::ClockEvent *event){
    CtrlRegisters *_this = (CtrlRegisters *)__this;
    std::cout << "[Debug Stop]: Stop simulation at cycle: " << _this->clock.get_cycles() << std::endl;
    _this->time.get_engine()->quit(1);
}


void CtrlRegisters::wakeup_event_handler(vp::Block *__this, vp::ClockEvent *event) {
    CtrlRegisters *_this = (CtrlRegisters *)__this;
    if (_this->hbm_preload_done)
    {
        if (_this->wakeup_timestamp_queue.size() > 0)
        {
            wakeup_payload_t wakeup_payload = _this->wakeup_timestamp_queue.front();
            //Check Timestamp
            if (_this->clock.get_cycles() >= wakeup_payload.timestamp)
            {
                //Check Barrier Type
                if (wakeup_payload.is_global)
                {
                    //Wakeup all clusters
                    for (int i = 0; i < _this->num_cluster_all; ++i)
                    {
                        _this->barrier_ack_itf_array[i].sync(1);
                    }
                    _this->trace.msg("Global Barrier at %d ns\n", _this->time.get_time()/1000);
                } else {
                    //Wakeup group of clusters
                    auto it = _this->sync_group_mapper.find(wakeup_payload.group_id);
                    if (it == _this->sync_group_mapper.end()) {
                        _this->trace.fatal("Group ID %0d Barrier Not Registered\n", wakeup_payload.group_id);
                    }
                    for (uint16_t cluster_id : it->second) {
                        _this->barrier_ack_itf_array[cluster_id].sync(1);
                    }
                    _this->trace.msg("Group %d Barrier at %d ns\n", wakeup_payload.group_id, _this->time.get_time()/1000);
                }
                _this->wakeup_timestamp_queue.pop();

                if (_this->wakeup_timestamp_queue.size() > 0)
                {
                    //Enqueue for next pending sync
                    _this->event_enqueue(_this->wakeup_event, _this->wakeup_timestamp_queue.front().timestamp - _this->clock.get_cycles());
                }
            } else {
                //Enqueue for next pending sync
                _this->event_enqueue(_this->wakeup_event, wakeup_payload.timestamp - _this->clock.get_cycles());
            }
        }
    } else {
        _this->event_enqueue(_this->wakeup_event, 300);
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
            wakeup_payload_t wakeup_payload;
            wakeup_payload.is_global = 1;
            wakeup_payload.timestamp = _this->clock.get_cycles() + _this->num_cluster_x + _this->num_cluster_y;
            _this->wakeup_timestamp_queue.push(wakeup_payload);
            _this->event_enqueue(_this->wakeup_event, 1);
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
        if (offset == 24) {
            // Reset sync_group_mapper
            _this->sync_group_mapper.clear();
            std::cout << "Sync group mapper reset.\n";
        } else if (offset == 28) {
            // Insert into sync_group_mapper
            uint16_t group_id = value & 0xFFFF; // Lower 16 bits
            uint16_t cluster_id = value >> 16; // Upper 16 bits

            auto& grp = _this->sync_group_mapper[group_id]; // Get or create the set for group_id
            auto result = grp.insert(cluster_id);   // Insert the cluster_id

            if (result.second) {
                std::cout << "Added Cluster ID " << cluster_id << " to Group ID " << group_id << "\n";
            } else {
                std::cout << "Cluster ID " << cluster_id << " already exists under Group ID " << group_id << "\n";
            }
        } else if (offset == 32) {
            // Display sync_group_mapper
            if (!_this->sync_group_mapper.empty()) {
                std::cout << "Displaying all groups and their clusters:\n";
                for (const auto& [group_id, cluster_set] : _this->sync_group_mapper) {
                    std::cout << "Group ID: " << group_id << "\n";
                    for (uint16_t cluster_id : cluster_set) {
                        std::cout << "  - Cluster ID: " << cluster_id << "\n";
                    }
                }
            } else {
                std::cout << "No groups or clusters available.\n";
            }
        } else if (offset == 36) {
            // Trigger cluster group synchronization
            uint16_t group_id = value & 0xFFFF; // Lower 16 bits
            wakeup_payload_t wakeup_payload;
            wakeup_payload.is_global = 0;
            wakeup_payload.group_id = group_id;
            wakeup_payload.timestamp = _this->clock.get_cycles() + _this->num_cluster_x + _this->num_cluster_y;
            _this->wakeup_timestamp_queue.push(wakeup_payload);
            _this->event_enqueue(_this->wakeup_event, 1);
        } else if (offset == 40) {
            //Stop at Time
            _this->event_enqueue(_this->debug_stop_event, value);
        }
    }

    return vp::IO_REQ_OK;
}

CtrlRegisters::~CtrlRegisters(){
    delete[] this->barrier_ack_itf_array;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new CtrlRegisters(config);
}