Tutorials
---------

0 - How to build a system from scratch
......................................

The goal of this tutorial is to build a simple system from scratch containing a core,
an interconnect and a memory.

The tutorial is located here: docs/developer/tutorials/0_how_to_build_a_system_from_scratch

The solution is under directory *solution*.

A simple runtime is available here: docs/developer/tutorials/utils

A makefile is available in the tutorial for compiling gvsoc, compiling the application to be
simulated, and running the simulation.

GVSOC can be compiled with *make gvsoc*, using this target: ::

    gvsoc:
        make -C ../../../.. TARGETS=my_system MODULES=$(CURDIR) build

It adds our local directory to the list of GVSOC modules so that it can find our system. This command is for now
failing as we need to write the script describing our system.

Compiling and running the application can be done with *make all run*. The run target also
adds our local directory to the list of target dirs so that it can find our system: ::

    run:
	    gvsoc --target-dir=$(CURDIR) --target=my_system --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test run $(runner_args)

Now, in order to build a system scratch, we need to write a python generator of the system which
will assemble all the pieces together.

First we need to declare our system as a gapy target, which is the runner used to run GVSOC
simulation:

.. code-block:: python

    import gvsoc.runner

    GAPY_TARGET = True

    class Target(gvsoc.runner.Target):

        def __init__(self, parser, options):
            super(Target, self).__init__(parser, options,
                model=Rv64, description="RV64 virtual board")

The target should aways inherit from *gvsoc.runner.Target* and we should always put
*GAPY_TARGET = True* in the file so that gapy knows this is a valid target when we put it on
gapy command-line.

The *model* argument should give the class of our system that we will describe now.

First we will put a top component whose role is to provide a clock to the rest of the system.
The top component must always inherit from *gvsoc.systree.Component*, and contain and propagate all the
following options:

.. code-block:: python

    class Rv64(gvsoc.systree.Component):

        def __init__(self, parent, name, parser, options):
            super().__init__(parent, name, options=options)

Only the parser option does not need to be propagated but can be used to declare or get options
from the command-line.

Now we can instantiate a clock generator and give it the initial frequency:

.. code-block:: python

    import vp.clock_domain

.. code-block:: python

    clock = vp.clock_domain.Clock_domain(self, 'clock', frequency=100000000)


Then let's instantiate our real system and connect its clock to the clock generator.
Since we connect the whole system to the clock generator, the same clock will be propagated
to all components in our system:

.. code-block:: python

    soc = Soc(self, 'soc', parser)
    clock.o_CLOCK    (soc.i_CLOCK    ())

Each component is providing methods for getting input ports and connecting output ports, that we
can use to connect our components together.

Now we can declare our real system:

.. code-block:: python

    class Soc(gvsoc.systree.Component):

        def __init__(self, parent, name, parser):
            super().__init__(parent, name)

            # Parse the arguments to get the path to the binary to be loaded
            [args, __] = parser.parse_known_args()

            binary = args.binary

We give it the top parser so that it can get the path of the binary to be simulated, which we
will use later for the loading.

We first instantiate the memory. We give it its size, which is passed as method parameter.
The Python generator of the memory component will declare it as a property, which will make sure
it is passed to the C++ model through its JSON configuration.

.. code-block:: python

    import memory.memory

.. code-block:: python

    mem = memory.memory.Memory(self, 'mem', size=0x00100000)

Then the interconnect. We bind the interco to the memory with a special binding, since we route
requests to the memory only for a certain range of the memory map.

.. code-block:: python

    import interco.router

.. code-block:: python

    ico = interco.router.Router(self, 'ico')
    ico.o_MAP(mem.i_INPUT(), 'mem', base=0x00000000, remove_offset=0x00000000, size=0x00100000)

The range is specified using *base* and *size*. The other argument, *remove_offset* can be used
to remap the base address of the requests, so that they arrive in the memory component with
a local offset.

Now we can add and bind the core. We take a default riscv core:

.. code-block:: python

    import cpu.iss.riscv

.. code-block:: python

    host = cpu.iss.riscv.Riscv(self, 'host', isa='rv64imafdc')
    host.o_FETCH     (ico.i_INPUT    ())
    host.o_DATA      (ico.i_INPUT    ())
    host.o_DATA_DEBUG(ico.i_INPUT    ())

We connect everything to the interconnect so that all requests are routed to the memory. The ISS
needs one port for fetching instructions and one for data, in case they need to follow different
paths for timing purpose. We also connect the debug port so that we can also connect GDB.

The next component is not modeling any hardware, but is just here to allow loading the binary to be
simulated.

.. code-block:: python

    import utils.loader.loader

.. code-block:: python

    loader = utils.loader.loader.ElfLoader(self, 'loader', binary=binary)
    loader.o_OUT     (ico.i_INPUT    ())
    loader.o_START   (host.i_FETCHEN ())
    loader.o_ENTRY   (host.i_ENTRY   ())

We need the loader because GVSOC does not provide any debug feature for loading binaries, as
everything should be simulated using the timing models.

The loader will issue requests to the memory to copy the binary sections. Once done, it will send
the boot address to the core and activate its fetch enable pin so that it can start executing
the binary.

The last component, which is optional, is to put a GDB server so that we can connect GDB to debug
our binary execution.

.. code-block:: python

    import gdbserver.gdbserver

.. code-block:: python

    gdbserver.gdbserver.Gdbserver(self, 'gdbserver')

Now we can compile gvsoc with *make gvsoc*. Since it will execute our script to know which components
should be built, it is possible that we get some Python errors at this point.

Then we can run the simulation with "make all run".

We can activate instruction traces to see what happened: ::

    make all run runner_args="--trace=insn"

In order to connect GDB, we can run the simulation with "make run runner_args=--gdbserver". This will
open an RSP socket and wait for gdb connection which can then be launcher from another
terminal with: ::

    riscv64-unknown-elf-gdb build/test
    (gdb) target remote:12345
    Remote debugging using :12345
    _start () at ../utils/crt0.S:5
    5	    la    x2, stack
    (gdb) break main
    Breakpoint 1 at 0x2c16: file main.c, line 5.
    (gdb) c
    Continuing.

    Breakpoint 1, main () at main.c:5
    5	    printf("Hello\n");
    (gdb)


1 - How to write a component from scratch
.........................................

The goal is this tutorial is to write a component from scratch, add it to our previous system,
and access it from the simulated binary.

For that, we modified the application and we now put in it an access to a dedicated region, that
we want to redirect to our component:

.. code-block:: cpp

    int main()
    {
        printf("Hello, got 0x%x from my comp\n", *(uint32_t *)0x20000000);
        return 0;
    }

First we need to create a python script called *my_comp.py* and declare our component in it:

.. code-block:: python

    import gvsoc.systree

    class MyComp(gvsoc.systree.Component):

        def __init__(self, parent: gvsoc.systree.Component, name: str, value: int):
            super().__init__(parent, name)

Python generators are always getting these *parent* and *name* options which needs to be given
to the parent class. The first one is giving the parent which instantiated this component, and
the second gives the name of the component within its parent scope.

Additional options can then be added to let the parent parametrize the instance of this component,
like previously the size of the memory. Here we add the *value* option to let the parent gives the
value to be read by the simulated binary.

Then we need to specify the source code of this component. Several sources can be given.
That is all we need to trigger the compilation of this component, the framework will automically
make sure a loadable library is produced for our component.

.. code-block:: python

    self.add_sources(['my_comp.cpp'])

It is also possible to give cflags. Both cflags and sources can depend on the component parameters,
the framework will make sure it compiles 2 differents libraries since the static code is different.

Since we added an option, we need to declare it as a property, so that it is added into the JSON
configuration of the component, and so that the code can retrieve it.

.. code-block:: python

    self.add_properties({
        "value": value
    })

Our component will have an input port to receive incoming requests. It is good to declare a method
for it so that it is easy for the upper component to know what needs to be bound:

.. code-block:: python

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')

The name of the interface here should corresponds to the one in the C++ code to declare the port.
The signature is just an information for the framework so that it can check that we are binding
ports of the same kind.

Now we need to write the C++ code. We have to first declare a class which inherits from
*vp::Component*:

.. code-block:: cpp

    class MyComp : public vp::Component
    {

    public:
        MyComp(vp::ComponentConf &config);

    };

    extern "C" vp::Component *gv_new(vp::ComponentConf &config)
    {
        return new MyComp(config);
    }

The argument passed to our class is just here to propagate it to the parent class.

A C wrapping function called *gv_new* is needed to let the framework instantiate our class when
the shared library containing our component is loaded.

Now we need to declare in our class an input port where the requests from the core will be received.
This port will be associated a method which will get called everytime a request must be handled.
This method must be static, and will receive the class instance as first argument and the request as
second argument. We can also add in the class a variable which will hold the value to be returned.

.. code-block:: cpp

    private:
        static vp::IoReqStatus handle_req(void *__this, vp::IoReq *req);

        vp::IoSlave input_itf;

        uint32_t value;

Now we must write the constructor of our class. This one should contain the declaration of our input port.
It is also the place where it can read the JSON configuration to get the parameters which were given to
our Python instance:

.. code-block:: cpp

    MyComp::MyComp(vp::ComponentConf &config)
        : vp::Component(config)
    {
        this->input_itf.set_req_meth(&MyComp::handle_req);
        this->new_slave_port("input", &this->input_itf);

        this->value = this->get_js_config()->get_child_int("value");
    }

Finally we can implement our port handler, whose role is to detect a read at offset 0 and returns
the value specified in the Python instance:

.. code-block:: cpp

    vp::IoReqStatus MyComp::handle_req(vp::Block *__this, vp::IoReq *req)
    {
        MyComp *_this = (MyComp *)__this;

        printf("Received request at offset 0x%lx, size 0x%lx, is_write %d\n",
            req->get_addr(), req->get_size(), req->get_is_write());
        if (!req->get_is_write() && req->get_addr() == 0 && req->get_size() == 4)
        {
            *(uint32_t *)req->get_data() = _this->value;
        }
        return vp::IO_REQ_OK;
    }

The cast is needed because this handler is static.

The last step is to add our component in our previous system and connect it on the interconnect so
that accesses at 0x20000000 are routed to it.

For that it must first be imported:

.. code-block:: python

    import my_comp

And then instantiated:

.. code-block:: python

    comp = my_comp.MyComp(self, 'my_comp', value=0x12345678)
    ico.o_MAP(comp.i_INPUT(), 'comp', base=0x20000000, size=0x00001000, rm_base=True)

We can now compile gvsoc. Since our component is included into the system, the framework will automatically
compile it.

We can compile and run the application, which should output: ::

    Received request at offset 0x0, size 0x4, is_write 0
    Hello, got 0x12345678 from my comp





2 - How to make components communicate together
...............................................

The goal of this tutorial is to create a second component and connect it to our previous component
so that they interact together.

For that, our previous component, when it receives the read from the core, will now also notify
the other component, through a wire interface. Then the new component will in turn reply to this
component through a second binding.

First, let's enrich our first component with 2 ports, one for sending the notification, and one for
receiving the result:

.. code-block:: python

    def o_NOTIF(self, itf: gsystree.SlaveItf):
        self.itf_bind('notif', itf, signature='wire<bool>')

    def i_RESULT(self) -> gsystree.SlaveItf:
        return gsystree.SlaveItf(self, 'result', signature='wire<MyResult>')

Both are using the wire interface, which is an interface which can be used for sending values to
another component. This interface is a template, so that the type of the value to be exchanged
can be chosen.

In our case, the notif interface with send a boolean, and the result will receive a custom class.

In the same Python script we can then describe our second component:

.. code-block:: python

    class MyComp2(gsystree.Component):

        def __init__(self, parent: gsystree.Component, name: str):

            super().__init__(parent, name)

            self.add_sources(['my_comp2.cpp'])

        def i_NOTIF(self) -> gsystree.SlaveItf:
            return gsystree.SlaveItf(self, 'notif', signature='wire<bool>')

        def o_RESULT(self, itf: gsystree.SlaveItf):
            self.itf_bind('result', itf, signature='wire<MyResult>')

It has same interfaces but reversed in direction.

Now we have to declare the two new interfaces in our first component:

.. code-block:: cpp

    this->new_master_port("notif", &this->notif_itf);

    this->result_itf.set_sync_meth(&MyComp::handle_result);
    this->new_slave_port("result", &this->result_itf);

Since the second one is a slave interface and will receive values, it also needs to be
associated a handler, which will get called when the other component is sending a value.

We can then modify our previous handler to also send a notification to the second component:

.. code-block:: cpp

    _this->notif_itf.sync(true);

The handler for the result port can then be declared and implemented:

.. code-block:: cpp

    static void handle_result(void *__this, MyClass *result);

.. code-block:: cpp

    void MyComp::handle_result(void *__this, MyClass *result)
    {
        printf("Received results %x %x\n", result->value0, result->value1);
    }

Note that the type of the value can be anything, including a custom class, like in our case.
This allows exchanging complex data between components.

The second component can then be implemented:

.. code-block:: cpp

    class MyComp : public vp::Component
    {

    public:
        MyComp(vp::ComponentConf &config);

    private:
        static void handle_notif(void *__this, bool value);
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



    void MyComp::handle_notif(void *__this, bool value)
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

Note that it will send the result immediately when it receives the notification, which means the first
component will receive a method call while it is calling another component.

This is a case which often happens for simulation speed reason, so everytime we call an interface, we have
to make sure that the internal state of the component is in a coherent state.

The final step is to instantiate the second component and bind it with the first one:

.. code-block:: python

        comp2 = my_comp.MyComp2(self, 'my_comp2')
        comp.o_NOTIF(comp2.i_NOTIF())
        comp2.o_RESULT(comp.i_RESULT())

Now we can compile and run to get: ::

    Received request at offset 0x0, size 0x4, is_write 0
    Received value 1
    Received results 11111111 22222222
    Hello, got 0x12345678 from my comp


3 - How to add system traces to a component
...........................................

The goal of this tutorial is to show how to add system traces into our components.

The trace must first be declared into our component class:

.. code-block:: c++

    vp::Trace trace;

Then, the trace must be activated, and given a name. This name is the one we will see in the path
of the trace when it is dumped, and is also the one used for selecting the trace on the command line.

.. code-block:: c++

    this->traces.new_trace("trace", &this->trace);

The trace can then be dumped from our model using this code that we put at the beginning of our request
handler, in order to show information about the request:

.. code-block:: c++

    vp::IoReqStatus MyComp::handle_req(vp::Block *__this, vp::IoReq *req)
    {
        MyComp *_this = (MyComp *)__this;

        _this->trace.msg(vp::TraceLevel::DEBUG, "Received request at offset 0x%lx, size 0x%lx, is_write %d\n",
            req->get_addr(), req->get_size(), req->get_is_write());

Once gvsoc has been recompiled, we can then activate all the traces of our component with this command: ::

    make all run runner_args="--trace=my_comp"

The value to the option *--trace* is a regular expression used to enable all traces whose path is matching this pattern.

It is also possible to activate instruction traces at the same time to see where is done the access: ::

    make all run runner_args="--trace=my_comp --trace=insn"

This should dump: ::

    32470000: 3247: [/soc/host/insn                 ] main:0                           M 0000000000002c26 lui                 a5, 0x20000000            a5=0000000020000000 
    32590000: 3259: [/soc/my_comp/trace             ] Received request at offset 0x0, size 0x4, is_write 0
    32590000: 3259: [/soc/host/insn                 ] main:0                           M 0000000000002c2a c.lw                a1, 0(a5)                 a1=0000000012345678  a5:0000000020000000  PA:0000000020000000 

4 - How to add VCD traces to a component
........................................

The goal of this tutorial is to show how to add VCD traces to our component so that its activity
can be monitored from a VCD viewer like GTKwave.

The easiest way to dump a VCD trace is to declare a signal that we will use to set the value which
will be displayed on the viewer.

For that we first have to declare it in our component class:

.. code-block:: c++

    vp::Signal<uint32_t> vcd_value;

The signal is a template. Its type is the one of the value which will store the value of the signal. This type should
have at least the width of the signal.

Then the signal must be declared, and given a name and a width. This name is the name we will see in
the viewer and the one we can use on the command line to enable the VCD trace associated to
this signal.

.. code-block:: c++

    MyComp::MyComp(vp::ComponentConf &config)
        : vp::Component(config), vcd_value(*this, "status", 32)

Then, the signal can be given a value with the *set* method. All the changes of values done through
this method will be seen on the VCD viewer.

One interesting feature is to call the *release* method on the signal in order to show it in high
impedance. This can be useful to show some kind of idleness.

In our example, our signal will just display the value written by the core, except for a certain value
which will be showed as high impedance:

.. code-block:: c++

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

Once GVSOC has been recompiled, we can activate VCD tracing and enable all events with this command: ::

    make all run runner_args="--vcd --event=.*"

This should suggest a GTKwave command to be launched.

Once GTKwave is opened, on the SST view on the left, our signal can be seen under soc->my_comp. It
can then be added to the view by clicking on "Append" on the bottom left.

In order to automatically put our signal into the cental view without having to pick the signal
from the signal view, we can also modify our component generator to include these lines:

.. code-block:: python

    def gen_gtkw(self, tree, comp_traces):

        if tree.get_view() == 'overview':
            tree.add_trace(self, self.name, vcd_signal='status[31:0]', tag='overview')


5 - How to add a register map in a component
............................................

The goal of this tutorial is to show how to implement a register map in a component.

For that we will first show how to model the registers by hands, and in a second step how to use
a script to generate the code to handle the register map from a markdown description of it.

First let's add it by hands. This is pretty simple. Our component is getting a function call to his
request handler everytime our component is accessed. The handler can get the offset of the access
from the request and determine from that which register is being accessed.

In our case, we will have 2 registers, one at offset 0x0 containing what we did before and a new one
at offset 0x4, which is returning the double of the value coming from Python generators.

.. code-block:: cpp

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
            }
            else if (req->get_addr() == 4)
            {
                if (!req->get_is_write())
                {
                    *(uint32_t *)req->get_data() = _this->value * 2;
                }
            }
        }
    }

Things are easy here because we only supports 32bits aligned accesses to our registers which is
quite common. This requires more work if we want to support 8bits or 16bits unaligned accesses, like doing some
memcopies.

To simplify, the *Register* object can be used to update the value
with a method, and then check his full value to impact the model. This will also provide at the same
time support for system traces and VCD traces.

For that it first need to be declared and configured, similarly to what we have done with the *Signal* object:

.. code-block:: cpp

    vp::Register<uint32_t> my_reg;

.. code-block:: cpp

    MyComp::MyComp(vp::ComponentConf &config)
        : vp::Component(config), vcd_value(*this, "status", 32), my_reg(*this, "my_reg", 32)

Then the following code can be added in the handler. It is checking that the beginning of the request falls
into the register area, and if so update the register value using the method. Then it can check the full value
of the register to take an action, just a printf in our example.

.. code-block:: cpp

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

The following code can be added into the simulated binary in order to access our new register:

.. code-block:: cpp

    *(uint32_t *)0x20000008 = 0x11223344;
    *(uint8_t *)0x20000009 = 0x77;

    printf("Hello, got 0x%x at 0x20000008\n", *(uint32_t *)0x20000008);

Then after recompiling gvsoc, we can see the activity in the register with these options: ::

    make all run runner_args="--trace=my_comp/my_reg --trace-level=trace"

Which should displays: ::

    128290000: 12829: [/soc/my_comp/my_reg/trace             ] Modified register (value: 0x11223344)
    128310000: 12831: [/soc/my_comp/my_reg/trace             ] Modified register (value: 0x11227744)
    Hit value
    Hello, got 0x11227744 at 0x20000008


Now let's have a look at a way of generating the register map. For that we will use the *regmap-gen*
script which comes with gvsoc and allows geenrating the register map code from a register map described
in a markdown file.

Lets' first add this rule in the makefile to generate the register map: ::

    regmap:
        regmap-gen --input-md regmap.md --header headers/mycomp

Now let's describe the register map. This file should first describe the set of registers of the
regmap. Then for each register, it should describe all the fields of the register. This will be used
to generate accessors for each field of each register, to make it easy for the mdoel to handle
the registers, and also to generate detailed traces.

.. code-block:: markdown

    # MyComp

    ## Description

    ## Registers

    | Register Name | Offset | Size | Default     | Description      |
    | ---           | ---    | ---  | ---         | ---              |
    | REG0          | 0x00   | 32   | 0x00000000  | Register 0       |
    | REG1          | 0x04   | 32   | 0x00000000  | Register 1       |

    ### REG0

    #### Fields

    | Field Name | Offset | Size  | Default | Description |
    | ---        | ---    | ---   | ---     | ---         |
    | FIELD0     | 0      | 8     | 0x00    | Field 0     |
    | FIELD1     | 8      | 24    | 0x00    | Field 1     |


    ### REG1

    #### Fields

    | Field Name | Offset | Size  | Default | Description |
    | ---        | ---    | ---   | ---     | ---         |
    | FIELD0     | 0      | 8     | 0x00    | Field 0     |
    | FIELD1     | 8      | 8     | 0x00    | Field 1     |
    | FIELD2     | 16     | 8     | 0x00    | Field 2     |
    | FIELD3     | 24     | 8     | 0x00    | Field 3     |

Once executed, the script should generate several files in the *header* directory, that we can include
into our model to instantiate the register map.

For that we must first include these two files:

.. code-block:: cpp

    #include "headers/mycomp_regfields.h"
    #include "headers/mycomp_gvsoc.h"

Then the register map must be declared into our component class:

.. code-block:: cpp

    vp_regmap_regmap regmap;

It must then be configured in our class constructor. A trace must be given, which will be used to trace
register accesses. We can use our class trace for that.

.. code-block:: cpp

    this->regmap.build(this, &this->trace);

In case we need to catch accesses to some registers when they are accessed, in order to take some actions,
we can attach a callback, which will get called everytime it is accessed:

.. code-block:: cpp

    this->regmap.reg0.register_callback(std::bind(
        &MyComp::handle_reg0_access, this, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3, std::placeholders::_4), true);

The callback needs to update the value of the register, and can do additional things:

.. code-block:: cpp

    void MyComp::handle_reg0_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
    {
        printf("REG0 callback\n");

        this->regmap.reg0.update(reg_offset, size, value, is_write);
    }

The last thing to do is to catch the accesses falling into the area managed by our new register map,
to forward them to it:

.. code-block:: cpp

    else if (req->get_addr() >= 0x100 && req->get_addr() < 0x200)
    {
        _this->regmap.access(req->get_addr() - 0x100, req->get_size(), req->get_data(),
            req->get_is_write());
        return vp::IO_REQ_OK;
    }

We can then add some accesses to the new register map in the simulated binary:

.. code-block:: cpp

    *(volatile uint32_t *)0x20000100 = 0x12345678;
    *(volatile uint32_t *)0x20000104 = 0x12345678;

Once GVSOC has been recompiled, and traces from our component has been enabled, we should see: ::

    177690000: 17769: [/soc/my_comp/trace                    ] Received request at offset 0x100, size 0x4, is_write 1
    REG0 callback
    177690000: 17769: [/soc/my_comp/reg0/trace               ] Modified register (value: 0x12345678)
    177690000: 17769: [/soc/my_comp/reg0/trace               ] Register access (name: REG0, offset: 0x0, size: 0x4, is_write: 0x1, value: { FIELD0=0x78, FIELD1=0x123456 })
    177840000: 17784: [/soc/my_comp/trace                    ] Received request at offset 0x104, size 0x4, is_write 1
    177840000: 17784: [/soc/my_comp/reg1/trace               ] Modified register (value: 0x12345678)
    177840000: 17784: [/soc/my_comp/reg1/trace               ] Register access (name: REG1, offset: 0x4, size: 0x4, is_write: 0x1, value: { FIELD0=0x78, FIELD1=0x56, FIELD2=0x34, FIELD3=0x12 })


6 - How to add timing
.....................

The goal of this tutorial is show how add basic timing to a component.

For that we will start from the 3rd tutorial where we made 2 components communicate together but
this time, the second component which receives the notification will wait some cycles before
sending back the result.

Delaying actions in a clocked component can be done using clock events. These are callbacks which are
registered to be executed after a certain amount of cycles has ellapsed.

For that, we must first declare the event in our class:

.. code-block:: cpp

    vp::ClockEvent event;

Then we have to configure it in the constructor:

.. code-block:: cpp

    MyComp::MyComp(vp::ComponentConf &config)
        : vp::Component(config), event(this, MyComp::handle_event)
    {

The method that we specify when we configure it, is the event callback, which will get called everytime
the event gets executed.

The event must be enqueued with a certain number of cycles, which tells in how many cycles from the
current cycle the event must be executed.

In our case, we will enqueue the event when we receive the notification, and we want that it executes
10 cycles after the access:

.. code-block:: cpp

    void MyComp::handle_notif(vp::Block *__this, bool value)
    {
        MyComp *_this = (MyComp *)__this;

        _this->trace.msg(vp::TraceLevel::DEBUG, "Received notif\n");

        if (!_this->event.is_enqueued())
        {
            _this->event.enqueue(10);
        }
    }

It is important to enqueue it only if it is not already the case because we could receive another
notification while the event is enqueued.

Then we need to write the callback of the event, which will send the result to the other component:

.. code-block:: cpp

    void MyComp::handle_event(vp::Block *__this, vp::ClockEvent *event)
    {
        MyComp *_this = (MyComp *)__this;
        _this->trace.msg(vp::TraceLevel::DEBUG, "Sending result\n");

        MyClass result = { .value0=0x11111111, .value1=0x22222222 };
        _this->result_itf.sync(&result);
    }

To see the effect, we can dump instruction traces and the ones from our component: ::

    32470000: 3247: [/soc/host/insn                  ] main:0                           M 0000000000002c26 lui                 a5, 0x20000000            a5=0000000020000000 
    Received request at offset 0x0, size 0x4, is_write 0
    32590000: 3259: [/soc/my_comp2/trace             ] Received notif
    32590000: 3259: [/soc/host/insn                  ] main:0                           M 0000000000002c2a c.lw                a1, 0(a5)                 a1=0000000012345678  a5:0000000020000000  PA:0000000020000000 
    32600000: 3260: [/soc/host/insn                  ] main:0                           M 0000000000002c2c c.addi              sp, sp, fffffffffffffff0  sp=0000000000000a70  sp:0000000000000a80 
    32610000: 3261: [/soc/host/insn                  ] main:0                           M 0000000000002c2e addi                a0, 0, 260                a0=0000000000000260 
    32620000: 3262: [/soc/host/insn                  ] main:0                           M 0000000000002c32 c.sdsp              ra, 8(sp)                 ra:0000000000000c0e  sp:0000000000000a70  PA:0000000000000a78 
    32690000: 3269: [/soc/my_comp2/trace             ] Sending result
    Received results 11111111 22222222
    32750000: 3275: [/soc/host/insn                  ] main:0                           M 0000000000002c34 jal                 ra, ffffffffffffffb4      ra=0000000000002c38 
    32770000: 3277: [/soc/host/insn                  ] printf:0                         M 0000000000002be8 c.addi16sp          sp, sp, ffffffffffffffa0  sp=0000000000000a10  sp:0000000000000a70 
    32780000: 3278: [/soc/host/insn                  ] printf:0                         M 0000000000002bea addi                t1, sp, 28                t1=0000000000000a38  sp:0000000000000a10 

As we can see on the traces, the results were indeed sent 10 cycles after we received the notification. We also
see that the core is executing instructions in between, which is what we want.

This way of executing callbacks at specific cycles may seem basic, but actually allow us to build
complex pipelines by executing the right actions at the right cycles.




7 - How to use the IO request interface
.......................................

The goal of this tutorial is to give more details about the IO interface used for exchanging
memory-mapped requests.

We will start from the 4th tutorial where we added system traces to our component and we will now see
different ways of handling the incoming requests.

First let's just add simple timing to our response:

.. code-block:: cpp

    if (req->get_size() == 4)
    {
        if (req->get_addr() == 0)
        {
            *(uint32_t *)req->get_data() = _this->value;
            req->inc_latency(1000);
            return vp::IO_REQ_OK;
        }
    }

This way of replying immediately to the request in the same function call is called a synchronous
reply. The master calls its IO interface and receives immediately the response.

In this case, we can add timing just by increasing the latency of the request. Even though the master
receives immediately the response, the number of cycles will tell him that the response actually took
the specified cycles, and it can take it into account into his pipeline. In our case the core will just
be stalled during the same number of cycles. The number of latency cycles that we set here will cumulate
with others added by other components on the same path from the initiator to the target.

To see the impact we can dump traces and get: ::

    32790000: 3279: [/soc/host/insn                 ] main:0                           M 0000000000002c2a lui                 s1, 0x20000000            s1=0000000020000000 
    32800000: 3280: [/soc/my_comp/trace             ] Received request at offset 0x0, size 0x4, is_write 0
    32800000: 3280: [/soc/host/insn                 ] main:0                           M 0000000000002c2e c.lw                a1, 0(s1)                 a1=0000000000000024  s1:0000000020000000  PA:0000000020000000 
    42810000: 4281: [/soc/host/insn                 ] main:0                           M 0000000000002c30 c.sdsp              s0, 10(sp)                s0:0000000000000010  sp:0000000000000a60  PA:0000000000000a70 
    42820000: 4282: [/soc/host/insn                 ] main:0                           M 0000000000002c32 addi                a0, 0, 260                a0=0000000000000260 

Now we are going to use an asynchronous reply, which allows modeling more complex handling of requests.
In particular this allows handling requests where the response time is unknown and depend on external factors,
like waiting for a cache refill.

We will add a second register where we handle the requests asynchronously. For that, instead of replying,
we store the requests, enqueue a clock event and return a different status, *vp::IO_REQ_PENDING*, which will
tell to the initiator of the request that the request is pending and that we will reply later:

.. code-block:: cpp

    else if (req->get_addr() == 4)
    {
        _this->pending_req = req;
        _this->event.enqueue(2000);
        return vp::IO_REQ_PENDING;
    }

The clock event is used to schedule a function call, which will reply to the request, by calling the
response method on the slave port where we received the request. We can do that with the following code:

.. code-block:: cpp

    void MyComp::handle_event(vp::Block *__this, vp::ClockEvent *event)
    {
        MyComp *_this = (MyComp *)__this;

        *(uint32_t *)_this->pending_req->get_data() = _this->value;
        _this->pending_req->get_resp_port()->resp(_this->pending_req);
    }

This will tell to the initiator that the response is ready. In our case, this will unstall the core
when it receives this call.

To see the effect we can dump traces: ::

    95000000: 9500: [/soc/host/insn                 ] printf:0                         M 0000000000002c14 c.jr                0, ra, 0, 0               ra:0000000000002c3c 
    95020000: 9502: [/soc/my_comp/trace             ] Received request at offset 0x4, size 0x4, is_write 0
    115020000: 11502: [/soc/host/insn                 ] main:0                           M 0000000000002c3c c.lw                a1, 4(s1)                 a1=0000000012345678  s1:0000000020000000  PA:0000000020000004 
    115030000: 11503: [/soc/host/insn                 ] main:0                           M 0000000000002c3e addi                a0, 0, 260                a0=0000000000000260 

This example is quite simple, but in practice, things are much more complex in case asynchronous replies
are used. A succession of several callbacks from different components are executed, until finally
the initiator receives the response.



8 - How to add mutiple cores
............................

The goal of this tutorial is to show how to modify our system generator to include a second core.

For that, we just need to instantiate a second time the same core and connect it the same way we did
for the first core:

.. code-block:: python

        # Instantiates the main core and connect fetch and data to the interconnect
        host2 = cpu.iss.riscv.Riscv(self, 'host2', isa='rv64imafdc', core_id=1)
        host2.o_FETCH     ( ico.i_INPUT     ())
        host2.o_DATA      ( ico.i_INPUT     ())
        host2.o_DATA_DEBUG(ico.i_INPUT    ())

We have to be carefull to give it a different name and a different core id, so that we can also
see it in gdb.

In order to make it execute the same binary, we also connect the loader to it:

.. code-block:: python

    loader.o_START   ( host2.i_FETCHEN  ())
    loader.o_ENTRY   ( host2.i_ENTRY    ())

Here we connect a single output of the loader to several inputs. In this case, any call made by the
loader to this interface will be broadcasted to all slaves, which is what we want, so that the 2 cores
get the binary entry point and start at the same time.

To see if both cores are really executing, we can check instruction traces: ::

    28420000: 2842: [/soc/host/insn                   ] $x:0                             M 0000000000000c12 auipc               sp, 0x0           sp=0000000000000c12 
    28420000: 2842: [/soc/host2/insn                  ] $x:0                             M 0000000000000c12 auipc               sp, 0x0           sp=0000000000000c12 
    28430000: 2843: [/soc/host/insn                   ] $x:0                             M 0000000000000c16 addi                sp, sp, fffffffffffffe7e  sp=0000000000000a90  sp:0000000000000c12 
    28440000: 2844: [/soc/host/insn                   ] $x:0                             M 0000000000000c1a auipc               t0, 0x0                   t0=0000000000000c1a 
    28450000: 2845: [/soc/host/insn                   ] $x:0                             M 0000000000000c1e addi                t0, t0, 1a                t0=0000000000000c34  t0:0000000000000c1a 
    28460000: 2846: [/soc/host/insn                   ] $x:0                             M 0000000000000c22 csrrw               0, t0, mtvec              t0:0000000000000c34 
    28470000: 2847: [/soc/host/insn                   ] $x:0                             M 0000000000000c26 auipc               t0, 0x0                   t0=0000000000000c26 
    28480000: 2848: [/soc/host/insn                   ] $x:0                             M 0000000000000c2a addi                t0, t0, ffffffffffffffc4  t0=0000000000000bea  t0:0000000000000c26 
    28490000: 2849: [/soc/host/insn                   ] $x:0                             M 0000000000000c2e c.li                a0, 0, 0                  a0=0000000000000000 
    28500000: 2850: [/soc/host/insn                   ] $x:0                             M 0000000000000c30 jalr                ra, t0, 0                 ra=0000000000000c34  t0:0000000000000bea 
    28520000: 2852: [/soc/host/insn                   ] __init_start:0                   M 0000000000000bea c.addi              sp, sp, fffffffffffffff0  sp=0000000000000a80  sp:0000000000000a90 
    28590000: 2859: [/soc/host2/insn                  ] $x:0                             M 0000000000000c16 addi                sp, sp, fffffffffffffe7e  sp=0000000000000a90  sp:0000000000000c12

We see indeed that both cores are executing together. They don't execute exactly the same because of
the default timing model of the memory, which create contentions between the 2 cores.

We can also check in GTKwave the multi-core execution.

GDB can also connected, and we'll see the 2 cores as 2 different threads: ::

    (gdb) info threads
    Id   Target Id         Frame 
    * 1    Thread 1 (host)   main () at main.c:6
    2    Thread 2 (host2)  0x0000000000000bfe in __init_do_ctors () at ../utils/init.c:16



9 - How to handle clock domains and frequency scaling
.....................................................

The goal of this tutorial is show how to modify our system to now have several clock domains.

We will create one clock domain containing the core, and another one containing the rest of the
components.

First we remove our top component containing our only clock domain and we replace
it with a top component containing everything. Then we create our 2 clock domains in it:

.. code-block:: python

    class Rv64(gsystree.Component):

        def __init__(self, parent, name, parser, options):

            super().__init__(parent, name, options=options)

            # Parse the arguments to get the path to the binary to be loaded
            [args, __] = parser.parse_known_args()

            binary = args.binary

            clock1 = Clock_domain(self, 'clock1', frequency=100000000)
            clock2 = Clock_domain(self, 'clock2', frequency=100000000)

Since the clock domains are now at the same level than the other components, we need to connect
their clock one by one:

.. code-block:: python

    clock1.o_CLOCK    ( host.i_CLOCK     ())
    clock2.o_CLOCK    ( ico.i_CLOCK     ())
    clock2.o_CLOCK    ( mem.i_CLOCK     ())
    clock2.o_CLOCK    ( comp.i_CLOCK     ())
    clock2.o_CLOCK    ( loader.i_CLOCK     ())

Note that the core is connected to a different clock domain compared to other components.
This will allows us to make it run at a different frequency.

The clock generators also have an input port for dynamically controlling their frequency.
We will connect our component to the control port of the first clock generator, so that it
can change its frequency:

.. code-block:: cpp

    comp.o_CLK_CTRL   (clock1.i_CTRL())

Now let's modify our component. We will use an event that we will periodically enqueue and change
the frequency everytime the event gets executed. To initiate that, we first enqueue it during
the reset, when it is deasserted:

.. code-block:: cpp

    void MyComp::reset(bool active)
    {
        if (!active)
        {
            this->frequency = 10000000;
            this->event.enqueue(100);
        }
    }

Then we use the handler to switch between 2 frequencies:

.. code-block:: cpp

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

Now we can see the effect by looking at the traces: ::

    66936000: 4566: [/host/insn                    ] __libc_prf_safe:0                M 0000000000001074 c.mv                a1, 0, s10                a1=0000000000000002 s10:0000000000000002 
    66937000: 4567: [/host/insn                    ] __libc_prf_safe:0                M 0000000000001076 c.jalr              ra, s9, 0, 0              ra=0000000000001078  s9:0000000000000e46 
    66939000: 4569: [/host/insn                    ] __libc_fputc_safe:0              M 0000000000000e46 c.lui               a3, 0x1000                a3=0000000000001000 
    67000000: 6700: [/my_comp/trace                ] Set frequency to 10000000
    69800000: 4658: [/host/insn                    ] __libc_fputc_safe:0              M 0000000000000e48 lw                  a4, fffffffffffffb18(a3)  a4=0000000000000006  a3:0000000000001000  PA:0000000000000b18 
    69900000: 4659: [/host/insn                    ] __libc_fputc_safe:0              M 0000000000000e4c c.lui               a5, 0x1000                a5=0000000000001000 

If we look the timestamp column, at the very left, we see that the duration of 1 clock cycle becomes
much longer after the frequency has changed.

We can also have a look in GTKwave to see that the cycle duration is periodically changing.




10 - How to customize an interconnect timing
............................................

The goal of this tutorial is to show how to customize the timing model of the interconnect.

The router that we are using in these tutorials have two ways of specifying the timing.

First, we can add a latency to all requests going through a mapping. This allows assigning
a different cost to each path in the router.

We add such a latency on the path from the core to the memory:

.. code-block:: python

    ico.o_MAP(mem.i_INPUT(), 'mem', base=0x00000000, size=0x00100000, rm_base=True, latency=100)

This should increase the latency of any access to the memory by 100 cycles, which will stall the core
during these cycles.

We can see the impact by looking at the instruction traces, where we can see the latency on each memory
access: ::

    45180000: 4518: [/soc/host/insn                 ] __init_bss:0                     M 0000000000000b4e bne                 a5, a3, fffffffffffffffa  a5:0000000000000a90  a3:0000000000000b00 
    45210000: 4521: [/soc/host/insn                 ] __init_bss:0                     M 0000000000000b48 sd                  0, 0(a5)                  a5:0000000000000a90  PA:0000000000000a90 
    46220000: 4622: [/soc/host/insn                 ] __init_bss:0                     M 0000000000000b4c c.addi              a5, a5, 8                 a5=0000000000000a98  a5:0000000000000a90 
    46230000: 4623: [/soc/host/insn                 ] __init_bss:0                     M 0000000000000b4e bne                 a5, a3, fffffffffffffffa  a5:0000000000000a98  a3:0000000000000b00 
    46260000: 4626: [/soc/host/insn                 ] __init_bss:0                     M 0000000000000b48 sd                  0, 0(a5)                  a5:0000000000000a98  PA:0000000000000a98 

The second way to modify the timing is to change the bandwidth of the router.

The bandwidth tells how many bytes per cycle can go through the router. To follow this rule, the
router computes the duration of each request based on its size and the bandwidth,
and sets the latencies of the requests so that in average the bandwidth is respected.

To see how it works, let's change the bdnwidth to 1 in our system. We also remove the latency that
we have put previously:

.. code-block:: python

    ico = interco.router.Router(self, 'ico', bandwidth=1)

Then we modify the simulated binary so that it does accesses in a loop:

.. code-block:: cpp

    for (int i=0; i<20; i++)
    {
        *(volatile uint32_t *)0x00000004;
        *(volatile uint32_t *)0x00000004;
        *(volatile uint32_t *)0x00000004;
        *(volatile uint32_t *)0x00000004;
    }

Then we can see the impact on traces: ::

    124860000: 12486: [/soc/host/insn                 ] main:0                           M 0000000000002c1a c.bnez              a5, 0, ffffffffffffffee   a5:0000000000000012 
    124890000: 12489: [/soc/host/insn                 ] main:0                           M 0000000000002c08 lw                  a4, 4(0)                  a4=0000000000000000   0:0000000000000000  PA:0000000000000004 
    124900000: 12490: [/soc/host/insn                 ] main:0                           M 0000000000002c0c lw                  a4, 4(0)                  a4=0000000000000000   0:0000000000000000  PA:0000000000000004 
    124940000: 12494: [/soc/host/insn                 ] main:0                           M 0000000000002c10 lw                  a4, 4(0)                  a4=0000000000000000   0:0000000000000000  PA:0000000000000004 
    124980000: 12498: [/soc/host/insn                 ] main:0                           M 0000000000002c14 lw                  a4, 4(0)                  a4=0000000000000000   0:0000000000000000  PA:0000000000000004 
    125020000: 12502: [/soc/host/insn                 ] main:0                           M 0000000000002c18 c.addiw             a5, a5, ffffffffffffffff  a5=0000000000000011  a5:0000000000000012 

The core should normally be able to do one load of 4 bytes per cycle, since the default bandwidth is 4,
but now, we see it is stalled starting on the second access.