#include <vp/vp.hpp>
#include <vp/itf/wire.hpp>
#include "my_class.hpp"

class MyComp : public vp::Component
{

public:
    MyComp(vp::ComponentConf &config);

private:
    static void handle_notif(vp::Block *__this, bool value);
    static void handle_event(vp::Block *_this, vp::ClockEvent *event);
    vp::WireSlave<bool> notif_itf;
    vp::WireMaster<MyClass *> result_itf;

    vp::ClockEvent event;
    vp::Trace trace;
};


MyComp::MyComp(vp::ComponentConf &config)
    : vp::Component(config), event(this, MyComp::handle_event)
{
    this->notif_itf.set_sync_meth(&MyComp::handle_notif);
    this->new_slave_port("notif", &this->notif_itf);

    this->new_master_port("result", &this->result_itf);

    this->traces.new_trace("trace", &this->trace);
}


void MyComp::handle_event(vp::Block *__this, vp::ClockEvent *event)
{
    MyComp *_this = (MyComp *)__this;
    _this->trace.msg(vp::TraceLevel::DEBUG, "Sending result\n");

    MyClass result = { .value0=0x11111111, .value1=0x22222222 };
    _this->result_itf.sync(&result);
}

void MyComp::handle_notif(vp::Block *__this, bool value)
{
    MyComp *_this = (MyComp *)__this;

    _this->trace.msg(vp::TraceLevel::DEBUG, "Received notif\n");

    _this->event.enqueue(10);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyComp(config);
}
