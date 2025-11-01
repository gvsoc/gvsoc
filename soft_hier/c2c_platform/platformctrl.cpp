/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
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

#include <vector>
#include <algorithm>
#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <pulp/snitch/snitch_cluster/cluster_periph_regfields.h>
#include <pulp/snitch/snitch_cluster/cluster_periph_gvsoc.h>


using namespace std::placeholders;


class PlatformCtrl : public vp::Component
{

public:

    PlatformCtrl(vp::ComponentConf &config);
    void reset(bool active);

private:
    static void barrier_sync(vp::Block *__this, bool value, int i);
    vp::Trace     trace;
    std::vector<vp::WireSlave<bool>> barrier_ack_itf;
    std::vector<vp::WireMaster<bool>> start_itf;
    uint32_t num_chip;
    std::vector<int> finished_list;
};

PlatformCtrl::PlatformCtrl(vp::ComponentConf &config)
: vp::Component(config)
{
    this->num_chip = this->get_js_config()->get("num_chip")->get_int();
    this->traces.new_trace("trace", &trace, vp::DEBUG);
    this->barrier_ack_itf.resize(this->num_chip);
    this->start_itf.resize(this->num_chip);
    for (int i = 0; i < (this->num_chip); ++i)
    {
        this->new_slave_port("barrier_ack_" + std::to_string(i), &this->barrier_ack_itf[i]);
        this->barrier_ack_itf[i].set_sync_meth_muxed(&PlatformCtrl::barrier_sync, i);
        this->finished_list.push_back(0);
        this->new_master_port("start_" + std::to_string(i), &this->start_itf[i]);
    }
}

void PlatformCtrl::barrier_sync(vp::Block *__this, bool value, int i)
{
    PlatformCtrl *_this = (PlatformCtrl *)__this;
    if (_this->finished_list[i] != 0)
    {
        _this->trace.fatal("[PlatformCtrl] Repeated ack on chip %d\n", i);
    }
    _this->finished_list[i] = 1;
    int all_ones = 1;
    for (int x : _this->finished_list) {
        if (x != 1) {
            all_ones = 0;
            break;
        }
    }
    if (all_ones)
    {
        std::cout << "[Performance Counter]: Execution period is " << (_this->time.get_time())/1000 << " ns" << std::endl;
        _this->time.get_engine()->quit(0);
    }
}

void PlatformCtrl::reset(bool active)
{
    if (active == 0)
    {
        std::cout << "[SystemInfo]: Start C2C Platform "  << std::endl;
        for (int i = 0; i < (this->num_chip); ++i)
        {
            this->start_itf[i].sync(1);
        }
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new PlatformCtrl(config);
}


