#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include "my_class.hpp"

class MyComp : public vp::Component
{

public:
    MyComp(vp::ComponentConf &config);

private:
    static vp::IoReqStatus handle_req(vp::Block *__this, vp::IoReq *req);
    static void handle_result(vp::Block *__this, MyClass *result);

    vp::IoSlave input_itf;
    vp::WireMaster<bool> notif_itf;
    vp::WireSlave<MyClass *> result_itf;

    uint32_t value;
};


MyComp::MyComp(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->input_itf.set_req_meth(&MyComp::handle_req);
    this->new_slave_port("input", &this->input_itf);

    this->new_master_port("notif", &this->notif_itf);

    this->result_itf.set_sync_meth(&MyComp::handle_result);
    this->new_slave_port("result", &this->result_itf);

    this->value = this->get_js_config()->get_child_int("value");

}

vp::IoReqStatus MyComp::handle_req(vp::Block *__this, vp::IoReq *req)
{
    MyComp *_this = (MyComp *)__this;

    printf("Received request at offset 0x%lx, size 0x%lx, is_write %d\n",
        req->get_addr(), req->get_size(), req->get_is_write());
    if (!req->get_is_write() && req->get_size() == 4)
    {
        *(uint32_t *)req->get_data() = _this->value;

        _this->notif_itf.sync(true);
    }
    return vp::IO_REQ_OK;
}

void MyComp::handle_result(vp::Block *__this, MyClass *result)
{
    printf("Received results %x %x\n", result->value0, result->value1);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyComp(config);
}
