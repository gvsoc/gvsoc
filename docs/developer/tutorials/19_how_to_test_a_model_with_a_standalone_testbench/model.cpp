#include <vp/vp.hpp>
#include <vp/itf/wire.hpp>


class Model : public vp::Component
{
public:
    Model(vp::ComponentConf &config);


private:
    static void sync(vp::Block *__this, int value);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    vp::WireMaster<int> output;
    vp::WireSlave<int> input;
    vp::ClockEvent fsm_event;
    int value;
};


Model::Model(vp::ComponentConf &config)
    : vp::Component(config), fsm_event(this, &this->fsm_handler)
{
    this->new_master_port("output", &this->output);

    this->input.set_sync_meth(&this->sync);
    this->new_slave_port("input", &this->input);
}

void Model::sync(vp::Block *__this, int value)
{
    Model *_this = (Model *)__this;
    _this->value = value;
    _this->fsm_event.enqueue(5);
}

void Model::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Model *_this = (Model *)__this;
    _this->output.sync(_this->value * 2);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Model(config);
}
