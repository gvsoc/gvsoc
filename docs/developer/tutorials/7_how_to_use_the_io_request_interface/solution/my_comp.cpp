#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>

class MyComp : public vp::Component
{

public:
    MyComp(vp::ComponentConf &config);

private:
    static vp::IoReqStatus handle_req(vp::Block *__this, vp::IoReq *req);
    static void handle_event(vp::Block *_this, vp::ClockEvent *event);
    void handle_reg0_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);

    vp::IoSlave input_itf;

    uint32_t value;

    vp::ClockEvent event;
    vp::Trace trace;

    vp::IoReq *pending_req;
};


MyComp::MyComp(vp::ComponentConf &config)
    : vp::Component(config), event(this, MyComp::handle_event)
{
    this->input_itf.set_req_meth(&MyComp::handle_req);
    this->new_slave_port("input", &this->input_itf);

    this->value = this->get_js_config()->get_child_int("value");

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
            *(uint32_t *)req->get_data() = _this->value;
            req->inc_latency(1000);
            return vp::IO_REQ_OK;
        }
        else if (req->get_addr() == 4)
        {
            _this->pending_req = req;
            _this->event.enqueue(2000);
            return vp::IO_REQ_PENDING;
        }
    }

    return vp::IO_REQ_INVALID;
}


void MyComp::handle_event(vp::Block *__this, vp::ClockEvent *event)
{
    MyComp *_this = (MyComp *)__this;

    *(uint32_t *)_this->pending_req->get_data() = _this->value;
    _this->pending_req->get_resp_port()->resp(_this->pending_req);
}



extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyComp(config);
}
