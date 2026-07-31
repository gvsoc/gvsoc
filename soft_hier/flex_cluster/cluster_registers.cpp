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
#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <pulp/snitch/snitch_cluster/cluster_periph_regfields.h>
#include <pulp/snitch/snitch_cluster/cluster_periph_gvsoc.h>


using namespace std::placeholders;


class ClusterRegisters : public vp::Component
{

public:

    ClusterRegisters(vp::ComponentConf &config);

    void reset(bool active);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    inline uint32_t get_nm_n() { return this->regmap.nm_config.format_n_get(); }
    inline uint32_t get_nm_m() { return this->regmap.nm_config.format_m_get(); }

private:
    static void barrier_sync(vp::Block *__this, bool value, int id);
    static vp::IoReqStatus global_barrier_sync(vp::Block *__this, vp::IoReq *req);
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void response(vp::Block *__this, vp::IoReq *req);
    static void hbm_preload_done_handler(vp::Block *__this, bool value);
    static void inst_preheat_done_handler(vp::Block *__this, bool value);
    void fetch_start_check();
    void cl_clint_set_req(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void cl_clint_clear_req(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void nm_config_req(uint64_t offset, int size, uint8_t *value, bool is_write);

    vp::Trace     trace;

    vp_regmap_cluster_periph regmap;

    vp::IoSlave in;
    uint32_t bootaddr;
    uint32_t status;
    int nb_cores;
    uint32_t cluster_id;
    vp::reg_32 barrier_status;
    uint32_t num_cluster_x;
    uint32_t num_cluster_y;

    std::vector<vp::WireSlave<bool>> barrier_req_itf;
    vp::WireMaster<bool> barrier_ack_itf;

    vp::IoSlave  global_barrier_slave_itf;
    vp::IoMaster global_barrier_master_itf;
    vp::IoReq*   global_barrier_master_req;
    uint32_t     global_barrier_addr;
    uint8_t *    global_barrier_buffer;

    vp::WireSlave<bool> hbm_preload_done_itf;
    vp::WireSlave<bool> inst_preheat_done_itf;
    vp::WireMaster<bool> fetch_start_itf;
    uint32_t hbm_preload_done;
    uint32_t inst_preheat_done;
    uint32_t fetch_started;

    std::vector<vp::WireMaster<bool>> external_irq_itf;

    vp::IoReq * global_barrier_query;
    int global_barrier_mutex;

    uint16_t global_sync_enable;
    uint64_t global_sync_timestamp;

};

ClusterRegisters::ClusterRegisters(vp::ComponentConf &config)
: vp::Component(config), regmap(*this, "regmap")
{
    this->traces.new_trace("trace", &trace, vp::DEBUG);

    this->in.set_req_meth(&ClusterRegisters::req);
    this->new_slave_port("input", &this->in);

    this->bootaddr = this->get_js_config()->get("boot_addr")->get_int();
    this->nb_cores = this->get_js_config()->get("nb_cores")->get_int();
    this->cluster_id = this->get_js_config()->get("cluster_id")->get_int();
    this->num_cluster_x = this->get_js_config()->get("num_cluster_x")->get_int();
    this->num_cluster_y = this->get_js_config()->get("num_cluster_y")->get_int();

    this->global_barrier_slave_itf.set_req_meth(&ClusterRegisters::global_barrier_sync);
    this->new_slave_port("global_barrier_slave", &this->global_barrier_slave_itf);
    this->new_master_port("global_barrier_master", &this->global_barrier_master_itf);
    this->global_barrier_master_req = this->global_barrier_master_itf.req_new(0, 0, 0, 0);
    this->global_barrier_addr = this->get_js_config()->get("global_barrier_addr")->get_int();
    this->global_barrier_buffer = new uint8_t[4];
    this->global_barrier_master_itf.set_resp_meth(&ClusterRegisters::response);
    this->global_barrier_master_itf.set_grant_meth(&ClusterRegisters::grant);

    this->global_barrier_query = NULL;
    this->global_barrier_mutex = 0;

    this->global_sync_enable = 0;
    this->global_sync_timestamp = 0;

    this->barrier_req_itf.resize(this->nb_cores);
    for (int i=0; i<this->nb_cores; i++)
    {
        this->barrier_req_itf[i].set_sync_meth_muxed(&ClusterRegisters::barrier_sync, i);
        this->new_slave_port("barrier_req_" + std::to_string(i), &this->barrier_req_itf[i]);
    }

    this->external_irq_itf.resize(this->nb_cores);
    for (int i=0; i<this->nb_cores; i++)
    {
        this->new_master_port("external_irq_" + std::to_string(i), &this->external_irq_itf[i]);
    }

    this->new_master_port("barrier_ack", &this->barrier_ack_itf);

    this->new_slave_port("hbm_preload_done", &this->hbm_preload_done_itf);
    this->new_slave_port("inst_preheat_done", &this->inst_preheat_done_itf);
    this->new_master_port("fetch_start", &this->fetch_start_itf);
    this->hbm_preload_done_itf.set_sync_meth(&ClusterRegisters::hbm_preload_done_handler);
    this->inst_preheat_done_itf.set_sync_meth(&ClusterRegisters::inst_preheat_done_handler);
    this->hbm_preload_done = 0;
    this->inst_preheat_done = 0;
    this->fetch_started = 0;

    this->regmap.build(this, &this->trace, "regmap");
    this->regmap.cl_clint_set.register_callback(std::bind(&ClusterRegisters::cl_clint_set_req, this, _1, _2, _3, _4));
    this->regmap.cl_clint_clear.register_callback(std::bind(&ClusterRegisters::cl_clint_clear_req, this, _1, _2, _3, _4));
    this->regmap.nm_config.register_callback(std::bind(&ClusterRegisters::nm_config_req, this, _1, _2, _3, _4));
}

vp::IoReqStatus ClusterRegisters::req(vp::Block *__this, vp::IoReq *req)
{
    ClusterRegisters *_this = (ClusterRegisters *)__this;
    uint64_t offset = req->get_addr();
    bool is_write = req->get_is_write();
    uint64_t size = req->get_size();
    uint32_t *data = (uint32_t *) req->get_data();

    // _this->trace.msg("Received IO req (offset: 0x%llx, size: 0x%llx, is_write: %d) -- Cluster ID: %0d\n", offset, size, is_write, _this->cluster_id);

    if (is_write && offset == 0 && _this->global_barrier_query == NULL)
    {
        if (_this->global_barrier_mutex == 1)
        {
            _this->global_barrier_mutex = 0;
            // _this->trace.msg("[Global Sync] Core Access <After> Globale Barrier\n");
            return vp::IO_REQ_OK;
        }else{
            _this->global_barrier_query = req;
            // _this->trace.msg("[Global Sync] Core Access <Before> Globale Barrier\n");
            return vp::IO_REQ_PENDING;
        }
    }

    if (!is_write && offset == 0)
    {
        data[0] = _this->cluster_id;
    }

    if(offset == 4){
        data[0] = 1;
    }

    if(offset == 8){
        data[0] = _this->num_cluster_x * _this->num_cluster_y;
    }

    if(offset == 12){
        data[0] = _this->num_cluster_x;
    }

    if(offset == 16){
        data[0] = _this->num_cluster_y;
    }

    if(is_write && offset == 20){
        if (_this->global_sync_enable == 0)
        {
            _this->global_sync_enable = 1;
            _this->global_sync_timestamp = _this->time.get_time()/1000;
        } else {
            uint32_t type = data[0];
            _this->global_sync_enable = 0;
            _this->trace.msg("Cluster Sync: %d ns -> %d ns | period = %d ns | Type = %d\n",
                _this->global_sync_timestamp,
                _this->time.get_time()/1000,
                _this->time.get_time()/1000 - _this->global_sync_timestamp,
                type);
        }
    }

    if(offset == 24){
        data[0] = 0;
    }

    if(is_write && offset == 28){
        uint32_t value = *(uint32_t *)data;
        uint8_t row_mask = value & 0xFFFF; // Lower 16 bits
        uint8_t col_mask = value >> 16; // Upper 16 bits

        // _this->trace.msg(vp::Trace::LEVEL_DEBUG, "[Global Sync] start wakeup with row_mask: %d and col_mask: %d\n", row_mask, col_mask);

        _this->global_barrier_master_req->prepare();
        _this->global_barrier_master_req->set_is_write(true);
        _this->global_barrier_master_req->set_addr(_this->global_barrier_addr);
        _this->global_barrier_master_req->set_size(4);
        _this->global_barrier_master_req->set_data(_this->global_barrier_buffer);

        uint8_t * payload_ptr = _this->global_barrier_master_req->get_payload();
        payload_ptr[0] = 1; //broadcast
        payload_ptr[1] = row_mask;
        payload_ptr[2] = col_mask;

        vp::IoReqStatus status = _this->global_barrier_master_itf.req(_this->global_barrier_master_req);

        if (status == vp::IO_REQ_INVALID)
        {
            _this->trace.fatal("[Global Sync] There was an error while broadcating wakeup signal\n");
        }
    }

    // _this->regmap.access(offset, size, data, is_write);

    return vp::IO_REQ_OK;
}

void ClusterRegisters::hbm_preload_done_handler(vp::Block *__this, bool value)
{
    ClusterRegisters *_this = (ClusterRegisters *)__this;
    _this->hbm_preload_done = 1;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "HBM Preloading Done\n");
    _this->fetch_start_check();
}

void ClusterRegisters::inst_preheat_done_handler(vp::Block *__this, bool value)
{
    ClusterRegisters *_this = (ClusterRegisters *)__this;
    _this->inst_preheat_done = 1;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Instruction Preheating Done\n");
    _this->fetch_start_check();
}

void ClusterRegisters::fetch_start_check()
{
    if (this->hbm_preload_done && this->inst_preheat_done)
    {
        if (this->fetch_started == 0)
        {
            this->fetch_start_itf.sync(1);
        }
    }
}

vp::IoReqStatus ClusterRegisters::global_barrier_sync(vp::Block *__this, vp::IoReq *req)
{
    ClusterRegisters *_this = (ClusterRegisters *)__this;

    // _this->trace.msg(vp::Trace::LEVEL_DEBUG, "[Global Sync] Globale Barrier Triggered\n");

    if (_this->global_barrier_query == NULL)
    {
        _this->global_barrier_mutex = 1;
    }else{
        _this->global_barrier_mutex = 0;
        _this->global_barrier_query->get_resp_port()->resp(_this->global_barrier_query);
        _this->global_barrier_query = NULL;
    }

    return vp::IO_REQ_OK;

}

void ClusterRegisters::barrier_sync(vp::Block *__this, bool value, int id)
{
    ClusterRegisters *_this = (ClusterRegisters *)__this;
    _this->barrier_status.set(_this->barrier_status.get() | (value << id));

    // _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Barrier sync (id: %d, status: 0x%x)\n", id, _this->barrier_status.get());

    if (_this->barrier_status.get() == (1ULL << _this->nb_cores) - 1)
    {
        // _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Barrier reached\n");

        _this->barrier_status.set(0);
        _this->barrier_ack_itf.sync(1);
    }
}

void ClusterRegisters::reset(bool active)
{
    this->new_reg("barrier_status", &this->barrier_status, 0, true);
}


void ClusterRegisters::cl_clint_set_req(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.cl_clint_set.update(reg_offset, size, value, is_write);
    for (int i=0; i<this->nb_cores; i++)
    {
        int irq_status = (this->regmap.cl_clint_set.get() >> i) & 1;
        if (irq_status == 1)
        {
            this->external_irq_itf[i].sync(true);
        }
    }
}

void ClusterRegisters::cl_clint_clear_req(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.cl_clint_clear.update(reg_offset, size, value, is_write);
    for (int i=0; i<this->nb_cores; i++)
    {
        int irq_status = (this->regmap.cl_clint_clear.get() >> i) & 1;
        if (irq_status == 1)
        {
            this->external_irq_itf[i].sync(false);
        }
    }
}

void ClusterRegisters::nm_config_req(uint64_t offset, int size, uint8_t *value, bool is_write)
{
    if (is_write)
    {
        // Let the register update its internal value
        this->regmap.nm_config.update(offset, size, value, is_write);

        // Read back the fields using the auto-generated accessors
        uint32_t n = this->regmap.nm_config.format_n_get();
        uint32_t m = this->regmap.nm_config.format_m_get();

        // Trace it
        this->trace.msg(vp::DEBUG, "NM_CONFIG write: N=%u, M=%u\n", n, m);
    }
    else
    {
        // Just forward to the register’s default read behavior
        this->regmap.nm_config.update(offset, size, value, is_write);
    }
}


void ClusterRegisters::response(vp::Block *__this, vp::IoReq *req)
{

}


void ClusterRegisters::grant(vp::Block *__this, vp::IoReq *req)
{

}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new ClusterRegisters(config);
}


