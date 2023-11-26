#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>

class MyComp : public vp::Component
{

public:
    MyComp(vp::ComponentConf &config);

    void reset(bool active);

private:
    static vp::IoReqStatus handle_req(vp::Block *__this, vp::IoReq *req);

    vp::IoSlave input_itf;

    vp::Trace trace;

    vp::WireMaster<int> power_ctrl_itf;
    vp::WireMaster<int> voltage_ctrl_itf;
};


MyComp::MyComp(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->input_itf.set_req_meth(&MyComp::handle_req);
    this->new_slave_port("input", &this->input_itf);

    this->new_master_port("power_ctrl", &this->power_ctrl_itf);

    this->new_master_port("voltage_ctrl", &this->voltage_ctrl_itf);

    this->traces.new_trace("trace", &this->trace);
}

vp::IoReqStatus MyComp::handle_req(vp::Block *__this, vp::IoReq *req)
{
    MyComp *_this = (MyComp *)__this;

    _this->trace.msg(vp::TraceLevel::DEBUG, "Received request at offset 0x%lx, size 0x%lx, is_write %d\n",
        req->get_addr(), req->get_size(), req->get_is_write());

    if (req->get_size() == 4)
    {
        if (req->get_addr() == 0)
        {
            if (req->get_is_write())
            {
                int power = (*(uint32_t *)req->get_data()) & 1;
                int clock = ((*(uint32_t *)req->get_data()) >> 1) & 1;

                int power_state;
                if (power)
                {
                    if (clock)
                    {
                        power_state = vp::PowerSupplyState::ON;
                     }
                    else
                    {
                        power_state = vp::PowerSupplyState::ON_CLOCK_GATED;
                    }
                }
                else
                {
                    power_state = vp::PowerSupplyState::OFF;

                }

                _this->power_ctrl_itf.sync(power_state);
            }
        }
        else if (req->get_addr() == 4)
        {
            if (req->get_is_write())
            {
                int voltage = *(uint32_t *)req->get_data();
                _this->voltage_ctrl_itf.sync(voltage);
            }
        }
    }

    return vp::IO_REQ_OK;
}


void MyComp::reset(bool active)
{
    if (active)
    {
        this->power_ctrl_itf.sync(vp::PowerSupplyState::OFF);
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyComp(config);
}
