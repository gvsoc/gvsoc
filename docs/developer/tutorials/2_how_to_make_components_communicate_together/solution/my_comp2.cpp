#include <vp/vp.hpp>
#include <vp/itf/wire.hpp>
#include "my_class.hpp"

class MyComp : public vp::Component
{

public:
    MyComp(vp::ComponentConf &config);

private:
    static void handle_notif(vp::Block *__this, bool value);
    vp::WireSlave<bool> notif_itf;
    vp::WireMaster<MyClass *> result_itf;
};


MyComp::MyComp(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->notif_itf.set_sync_meth(&MyComp::handle_notif);
    this->new_slave_port("notif", &this->notif_itf);

    this->new_master_port("result", &this->result_itf);
}



void MyComp::handle_notif(vp::Block *__this, bool value)
{
    MyComp *_this = (MyComp *)__this;

    printf("Received value %d\n", value);

    MyClass result = { .value0=0x11111111, .value1=0x22222222 };
    _this->result_itf.sync(&result);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyComp(config);
}
