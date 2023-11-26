#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>

class MyComp : public vp::Component
{

public:
    MyComp(vp::ComponentConf &config);

private:
    static vp::IoReqStatus handle_req(vp::Block *__this, vp::IoReq *req);
    void power_supply_set(vp::PowerSupplyState state);

    vp::IoSlave input_itf;

    uint32_t value;

    vp::Trace trace;
    vp::Signal<uint32_t> vcd_value;

    vp::PowerSource access_power;
    vp::PowerSource background_power;
};


MyComp::MyComp(vp::ComponentConf &config)
    : vp::Component(config), vcd_value(*this, "status", 32)
{
    this->input_itf.set_req_meth(&MyComp::handle_req);
    this->new_slave_port("input", &this->input_itf);

    this->value = this->get_js_config()->get_child_int("value");

    this->traces.new_trace("trace", &this->trace);

    this->power.new_power_source("leakage", &background_power, this->get_js_config()->get("**/background_power"));
    this->power.new_power_source("access", &access_power, this->get_js_config()->get("**/access_power"));

    this->background_power.leakage_power_start();
}

void MyComp::power_supply_set(vp::PowerSupplyState state)
{
    if (state == vp::PowerSupplyState::ON)
    {
        this->background_power.dynamic_power_start();

    }
    else
    {
        this->background_power.dynamic_power_stop();
    }
}

vp::IoReqStatus MyComp::handle_req(vp::Block *__this, vp::IoReq *req)
{
    MyComp *_this = (MyComp *)__this;

    _this->trace.msg(vp::TraceLevel::DEBUG, "Received request at offset 0x%lx, size 0x%lx, is_write %d\n",
        req->get_addr(), req->get_size(), req->get_is_write());

    _this->access_power.account_energy_quantum();

    if (req->get_size() == 4)
    {
        if (req->get_addr() == 0)
        {
            if (!req->get_is_write())
            {
                *(uint32_t *)req->get_data() = _this->value;
            }
        }
    }

    return vp::IO_REQ_OK;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyComp(config);
}
