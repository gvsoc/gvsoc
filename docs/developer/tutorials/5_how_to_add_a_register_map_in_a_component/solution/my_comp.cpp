#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/itf/io.hpp>
#include "headers/mycomp_regfields.h"
#include "headers/mycomp_gvsoc.h"

class MyComp : public vp::Component
{

public:
    MyComp(vp::ComponentConf &config);

private:
    static vp::IoReqStatus handle_req(vp::Block *__this, vp::IoReq *req);
    void handle_reg0_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);

    vp::IoSlave input_itf;

    uint32_t value;

    vp::Trace trace;
    vp::Signal<uint32_t> vcd_value;

    vp::Register<uint32_t> my_reg;

    vp_regmap_regmap regmap;
};


MyComp::MyComp(vp::ComponentConf &config)
    : vp::Component(config), vcd_value(*this, "status", 32), regmap(*this, "regmap"),
    my_reg(*this, "my_reg", 32)
{
    this->input_itf.set_req_meth(&MyComp::handle_req);
    this->new_slave_port("input", &this->input_itf);

    this->value = this->get_js_config()->get_child_int("value");

    this->traces.new_trace("trace", &this->trace);

    this->regmap.build(this, &this->trace);

    this->regmap.reg0.register_callback(std::bind(
        &MyComp::handle_reg0_access, this, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3, std::placeholders::_4), true);
}

void MyComp::handle_reg0_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    printf("REG0 callback\n");

    this->regmap.reg0.update(reg_offset, size, value, is_write);
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
            return vp::IO_REQ_OK;
        }
        else if (req->get_addr() == 4)
        {
            if (!req->get_is_write())
            {
                *(uint32_t *)req->get_data() = _this->value * 2;
            }
            return vp::IO_REQ_OK;
        }
        else if (req->get_addr() >= 0x100 && req->get_addr() < 0x200)
        {
            _this->regmap.access(req->get_addr() - 0x100, req->get_size(), req->get_data(),
                req->get_is_write());
            return vp::IO_REQ_OK;
        }
    }

    if (req->get_addr() >= 8 && req->get_addr() < 12)
    {
        _this->my_reg.update(req->get_addr() - 8, req->get_size(), req->get_data(),
            req->get_is_write());

        if (req->get_is_write() && _this->my_reg.get() == 0x11227744)
        {
            printf("Hit value\n");
        }

        return vp::IO_REQ_OK;
    }

    return vp::IO_REQ_INVALID;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new MyComp(config);
}
