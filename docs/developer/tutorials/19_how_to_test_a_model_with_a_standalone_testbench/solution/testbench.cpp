#include <vp/vp.hpp>
#include <vp/itf/wire.hpp>


class Testbench : public vp::Component
{
public:
    Testbench(vp::ComponentConf &config);


private:
    static void sync(vp::Block *__this, int value);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    void reset(bool active);

    vp::WireMaster<int> dut_input;
    vp::WireSlave<int> dut_output;
    vp::ClockEvent fsm_event;
    int64_t cycle_stamp;
};


Testbench::Testbench(vp::ComponentConf &config)
    : vp::Component(config), fsm_event(this, &this->fsm_handler)
{
    this->new_master_port("dut_input", &this->dut_input);

    this->dut_output.set_sync_meth(&this->sync);
    this->new_slave_port("dut_output", &this->dut_output);
}

void Testbench::reset(bool active)
{
    if (!active)
    {
        this->fsm_event.enqueue();
    }
}

void Testbench::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Testbench *_this = (Testbench *)__this;
    int value = 0x12345678;
    printf("Testbench sending value 0x%llx\n", value);
    _this->cycle_stamp = _this->clock.get_cycles();
    _this->dut_input.sync(value);
}

void Testbench::sync(vp::Block *__this, int value)
{
    Testbench *_this = (Testbench *)__this;

    printf("Testbench sending value 0x%llx\n", value);

    int status = value != 0x2468acf0 || _this->clock.get_cycles() - _this->cycle_stamp != 5;

    _this->time.get_engine()->quit(status);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Testbench(config);
}
