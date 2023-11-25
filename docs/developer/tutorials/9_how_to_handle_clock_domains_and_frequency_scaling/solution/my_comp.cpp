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
    static void handle_event(vp::Block *_this, vp::ClockEvent *event);

    vp::IoSlave input_itf;

    uint32_t value;

    vp::Trace trace;
    vp::Signal<uint32_t> vcd_value;
    vp::ClockMaster  clk_ctrl_itf;
    vp::ClockEvent event;

    int frequency;
};


MyComp::MyComp(vp::ComponentConf &config)
    : vp::Component(config), vcd_value(*this, "status", 32), event(this, MyComp::handle_event)
{
    this->input_itf.set_req_meth(&MyComp::handle_req);
    this->new_slave_port("input", &this->input_itf);

    this->value = this->get_js_config()->get_child_int("value");

    this->traces.new_trace("trace", &this->trace);

    new_master_port("clk_ctrl", &this->clk_ctrl_itf);

    this->frequency = frequency;
}


void MyComp::reset(bool active)
{
    if (!active)
    {
        this->frequency = 10000000;
        this->event.enqueue(100);
    }
}

vp::IoReqStatus MyComp::handle_req(vp::Block *__this, vp::IoReq *req)
{
    MyComp *_this = (MyComp *)__this;

    _this->trace.msg(vp::TraceLevel::DEBUG, "Received request at offset 0x%lx, size 0x%lx, is_write %d\n",
        req->get_addr(), req->get_size(), req->get_is_write());

    if (req->get_size() == 4)
    {
        if (!req->get_is_write())
        {
            *(uint32_t *)req->get_data() = _this->value;
        }
        else
        {
            uint32_t value = *(uint32_t *)req->get_data();
            if (value == 5)
            {
                _this->vcd_value.release();
            }
            else
            {
                _this->vcd_value.set(value);
            }
        }
    }

    return vp::IO_REQ_OK;
}


void MyComp::handle_event(vp::Block *__this, vp::ClockEvent *event)
{
    MyComp *_this = (MyComp *)__this;

    _this->trace.msg(vp::TraceLevel::DEBUG, "Set frequency to %d\n", _this->frequency);
    _this->clk_ctrl_itf.set_frequency(_this->frequency);

    if (_this->frequency == 10000000)
    {
        _this->frequency = 1000000000;
        _this->event.enqueue(1000);
    }
    else
    {
        _this->frequency = 10000000;
        _this->event.enqueue(100);
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyComp(config);
}
