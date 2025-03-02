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
#include <cstdio>
#include <string>


class UtilDumpper : public vp::Component
{

public:
    UtilDumpper(vp::ComponentConf &config);

private:
    static vp::IoReqStatus input_req(vp::Block *__this, vp::IoReq *req);
    static vp::IoReqStatus ctrl_req(vp::Block *__this, vp::IoReq *req);

    vp::Trace trace;
    vp::IoSlave input_itf;
    vp::IoSlave ctrl_itf;
    int dumpper_id;
    std::string filename;
    FILE* dump_file;
    uint64_t base_lower;
    uint64_t base_upper;
};



UtilDumpper::UtilDumpper(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->dumpper_id = this->get_js_config()->get("dumpper_id")->get_int();
    this->filename = "dump_" + std::to_string(this->dumpper_id);
    this->dump_file = nullptr;
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->input_itf.set_req_meth(&UtilDumpper::input_req);
    this->ctrl_itf.set_req_meth(&UtilDumpper::ctrl_req);
    this->new_slave_port("input", &this->input_itf);
    this->new_slave_port("ctrl", &this->ctrl_itf);
}



vp::IoReqStatus UtilDumpper::ctrl_req(vp::Block *__this, vp::IoReq *req)
{
    UtilDumpper *_this = (UtilDumpper *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    if (offset == 0 && is_write)
    {
        //open file
        if (_this->dump_file == nullptr)
        {
            _this->dump_file = fopen(_this->filename.c_str(), "w");
            if (!_this->dump_file)
            {
                _this->trace.fatal("[UtilDumpper] Failed to open dump file\n");
            }
        }
    } else if (offset == 4 && is_write)
    {
        //insert new base
        if (_this->dump_file)
        {
            uint64_t base_addr = (_this->base_upper << 32) | _this->base_lower;
            fprintf(_this->dump_file, "\n");
            fprintf(_this->dump_file, "HBM offset == .0x%016llx:\n", base_addr);
        }
    } else if (offset == 8 && is_write)
    {
        //close
        if (_this->dump_file)
        {
            fclose(_this->dump_file);
            _this->dump_file = nullptr;
        }
    } else if (offset == 12 && is_write)
    {
        //set lower half base address
        uint32_t value = *(uint32_t *)data;
        _this->base_lower = (uint64_t)value;
    } else if (offset == 16 && is_write)
    {
        //set upper half base address
        uint32_t value = *(uint32_t *)data;
        _this->base_upper = (uint64_t)value;
    }

    return vp::IO_REQ_OK;
}

vp::IoReqStatus UtilDumpper::input_req(vp::Block *__this, vp::IoReq *req)
{
    UtilDumpper *_this = (UtilDumpper *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();
    bool is_write = req->get_is_write();

    if (is_write && size > 1)
    {
        if (_this->dump_file)
        {
            uint16_t* ptr = (uint16_t *)data;
            uint32_t len = size / 2;
            for (int i = 0; i < len; ++i)
            {
                fprintf(_this->dump_file, "  0x%04x\n", ptr[i]);
            }
        }
    }

    return vp::IO_REQ_OK;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new UtilDumpper(config);
}