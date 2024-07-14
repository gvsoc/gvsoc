5. Introducing the FSM
^^^^^^^^^^^^^^^^^^^^^^^

Start by setting up the source for this task:

.. admonition:: Task - 3.5.1 Setup task source files 
   :class: task
   
   .. code-block:: bash
        
        $ make model_hwpe_task4


Open ``hwpe_fsm.cpp`` file. Do you notice any difference compared to Task - 3.4?

We added a register, ``vp::reg32 state``, to the ``hwpe.hpp`` file. The state is set to ``START`` in ``FsmStartHandler`` defined in ``hwpe_fsm.cpp``. The type declaration of ``HwpeState`` can be found in ``datatype.hpp``, and the ``state`` variable is declared in ``hwpe.hpp``.

Code Explanation:
   - ``FsmStartHandler`` calls ``fsm_loop()``.
   - ``fsm_loop()`` calls the ``fsm()`` function in a while loop until a latency > 0 is returned from ``fsm()`` or the state reaches ``END``.
   - When latency > 0, a new event is enqueued, either ``fsm_event`` or ``fsm_end_event``, depending on the next state.
   - The FSM is updated with helper functions and debug messages to print the states using ``HwpeStateToString``.


.. admonition:: Task - 3.5.2 Build and Run
   :class: task
   
   Run the GVSoC model by executing:
   
   .. code-block:: bash
    
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary ./tutorial/model_hwpe/application/task4/test run --trace="hwpe"

**Warning! It may lead to an infinite loop. Remember to terminate the process.**

What prints do you see from the trace? Does the prints looks like the ones below?

.. code-block:: none

   2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_INPUT finished with latency : 0 cycles
   2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_INPUT finished with latency : 0 cycles


.. admonition:: Task - 3.5.3 Reasoning about non-terminating code
   :class: task
   
   Can you guess what just happened? The FSM is stuck in an infinite loop. Why is this the case?
   
   Hint: Check the ``hwpe_fsm.cpp`` file. The exit condition ``input.iteration == iteration.count`` is never met for the ``LOAD_INPUT`` state. Uncomment the trace for ``iteration`` and ``count`` in the FSM. Build the GVSoC model and run the application. You will observe ``iteration=-1`` and ``count=0``.

This brings us to the next task. Initialize the values correctly. If you remember in Section 3.3, we discussed about the ``clear()`` function that we will implement in later tasks. Now is the time!

.. admonition:: Task - 3.5.3 Fix the initialization
   :class: task
   
   Open the ``hwpe.cpp`` and assign
   
   .. code-block:: cpp
    
        this->input.iteration = 0
        this->input.count = 8

Now the initial values are set correctly. But we also have to ensure ``input.iteration`` is incremented as expected. Have a look at the ``input_load.cpp`` file. You will see it instantiates the ``input_load()`` function that takes care of the data load where ``input.iteration`` is also incremented.

.. admonition:: Task - 3.5.4 Fix the non-terminating code
   :class: task
   
   Open ``hwpe_fsm.cpp`` and add a call to ``input_load()`` in the modified ``LOAD_INPUT`` case in the FSM. The code should look like the following
   
   .. code-block:: cpp
    
        case LOAD_INPUT:
            latency = this->input_load();
            if (this->input.iteration == this->input.count)
                state_next = WEIGHT_OFFSET;
        break;
   

.. admonition:: Verify - Is the code terminating now?
   :class: solution
   
   Build and execute the application
   
   .. code-block:: bash
    
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary ./tutorial/model_hwpe/application/task4/test run --trace="hwpe"


.. admonition:: Task - 3.5 Expected Traces
   :class: explanation

        .. code-block:: none

            2384905954: 171798: [/chip/cluster/hwpe/trace] ********************* First event enqueued *********************
            2384919836: 171799: [/chip/cluster/hwpe/trace] PRINTING CONFIGURATION REGISTER
            regconfig_manager >> INPUT POINTER : 0x1000001c
            regconfig_manager >> WEIGHT POINTER : 0x10000024
            regconfig_manager >> OUTPUT POINTER : 0x1000002c
            regconfig_manager >> WOFFS VALUE : 0xffffff80
            2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state START finished with latency : 0 cycles
            2384919836: 171799: [/chip/cluster/hwpe/trace] Input load for addr=0x1c, data=0x11
            2384919836: 171799: [/chip/cluster/hwpe/trace] Input load for addr=0x1d, data=0x22
            2384919836: 171799: [/chip/cluster/hwpe/trace] Input load for addr=0x1e, data=0x33
            2384919836: 171799: [/chip/cluster/hwpe/trace] Input load for addr=0x1f, data=0x44
            2384919836: 171799: [/chip/cluster/hwpe/trace] Input load for addr=0x20, data=0x55
            2384919836: 171799: [/chip/cluster/hwpe/trace] Input load for addr=0x21, data=0x66
            2384919836: 171799: [/chip/cluster/hwpe/trace] Input load for addr=0x22, data=0x77
            2384919836: 171799: [/chip/cluster/hwpe/trace] Input load for addr=0x23, data=0x88
            2384919836: 171799: [/chip/cluster/hwpe/trace] iteration=8, count=8
            2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_INPUT finished with latency : 4 cycles
            2384975364: 171803: [/chip/cluster/hwpe/trace] (fsm state) current state WEIGHT_OFFSET finished with latency : 0 cycles
            2384975364: 171803: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_WEIGHT finished with latency : 0 cycles
            2384975364: 171803: [/chip/cluster/hwpe/trace] (fsm state) current state COMPUTE finished with latency : 0 cycles
            2384975364: 171803: [/chip/cluster/hwpe/trace] (fsm state) current state STORE_OUTPUT finished with latency : 1 cycles
            Test success.
