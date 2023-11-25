
Block object
============

Overview
........

.. image:: images/block.png

Lifecycle methods
.................


.. code-block:: cpp

    virtual void reset(bool active) {}

    virtual void start() {}

    virtual void stop() {}

    virtual void flush() {}

    virtual void *external_bind(std::string path, std::string itf_name, void *handle);

    virtual std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file,
        std::vector<std::string> args, std::string req) { return ""; }

    virtual void power_supply_set(vp::PowerSupplyState state) {}

.. list-table:: Available port signature
   :header-rows: 1

   * - Name
     - Description
   * - reset
     - Called anytime the value of the reset is changed. Can be overloaded to do some
       initializations and trigger something when it is released.
   * - start
     - Called after the system has been instantiated and before the reset. Can be overloaded in
       order to interact with other components through ports, or to instantiate objects like
       threads which require the system to be instantiated.
   * - stop
     - Called when the simulation is over. Can be overloaded to close some objects like files.
   * - flush
     - Called anytime an interactive action happens like the profiler paused the simulation or
       a breakpoint was reached. Can be overloaded to make something visible to the user, like
       flushing stdout.
   * - external_bind
     - Called when an external loaded wants to be bound to an internal component.
   * - handle_command
     - Called when the GVSOC proxy needs this component to handle a custom command.
   * - power_supply_set
     - Called when the power supplied has changed. Can be overloaded to take some actions
       like changing power consumption.


.. code-block:: cpp

    void Gdb_server::start()
    {
        if (this->rsp)
        {
            this->rsp->start(this->get_js_config()->get_child_int("port"));
        }
    }

.. code-block:: cpp

    void Memory::reset(bool active)
    {
        if (active)
        {
            this->status.set(0);
        }
        else
        {
            this->notif_itf.sync(true);
        }
    }

Event-based modeling
....................

.. image:: images/event_based_model.png


Clock model
...........

Overview
########

.. image:: images/clock_domains.png

Clock events
############

.. code-block:: cpp

    class MyComp : public vp::Component
    {

    public:
        MyComp(vp::ComponentConf &config);

    private:
        static vp::IoReqStatus handle_request(vp::Block *__this, vp::IoReq *req);
        static void handle_event(vp::Block *_this, vp::ClockEvent *event);

        vp::ClockEvent event;
    };

.. code-block:: cpp

    MyComp::MyComp(vp::ComponentConf &config)
        : vp::Component(config), event(this, MyComp::handle_event)
    {
    }

.. code-block:: cpp

    vp::IoReqStatus MyComp::handle_request(vp::Block *__this, vp::IoReq *req)
    {
        MyComp *_this = (MyComp *)__this;

        _this->queue.push(req)
        _this->event.enqueue(10);

        return vp::IO_REQ_PENDING;
    }

    void MyComp::handle_event(vp::Block *__this, vp::ClockEvent *event)
    {
        MyComp *_this = (MyComp *)__this;

        vp::IoReq *req = _this->queue.pop();
        req->get_resp_port()->resp(req);
    }

.. code-block:: cpp

    void Exec::reset(bool active)
    {
        if (active)
        {
            this->instr_event->disable();
        }
        else
        {
            this->instr_event->enable();
        }
    }

.. code-block:: cpp

    inline void Timing::stall_cycles_account(int cycles)
    {
        this->iss.exec.instr_event->stall_cycle_inc(cycles);
    }

.. code-block:: cpp

        inline void set_callback(ClockEventMeth *meth);
        inline void **get_args();
        inline void exec();

        inline void enqueue(int64_t cycles = 1);
        inline void cancel();
        inline bool is_enqueued();

        inline void enable();
        inline void disable();
        inline void stall_cycle_set(int64_t value);
        inline void stall_cycle_inc(int64_t inc);
        inline int64_t stall_cycle_get();

Stubs
#####



Asynchronous blocks
...................


Overview
########

.. image:: images/time_events.png

Time events
###########

.. code-block:: cpp

    class MyComp : public vp::Component
    {

    public:
        MyComp(vp::ComponentConf &config);

    private:
        static void handle_event(vp::Block *_this, vp::TimeEvent *event);

        vp::TimeEvent event;
    };

.. code-block:: cpp

    MyComp::MyComp(vp::ComponentConf &config)
        : vp::Component(config), event(this, MyComp::handle_event)
    {
    }

.. code-block:: cpp

    void Exec::reset(bool active)
    {
        if (!active)
        {
            this->event.enqueue(10000);
        }
    }

    void MyComp::handle_event(vp::Block *__this, vp::ClockEvent *event)
    {
        MyComp *_this = (MyComp *)__this;
        _this->event.enqueue(10000);
    }

.. code-block:: cpp

        inline void set_callback(TimeEventMeth *meth);
        inline void **get_args();

        inline void enqueue(int64_t time);
        inline bool is_enqueued();
