7. Optimize Load Operation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Start the source setup by running:

.. admonition:: Task - 3.7.1 Setup task source files 
   :class: task
   
   .. code-block:: bash
        
        $ make model_hwpe_task7


Build the model and run the ``task4`` application. You will notice that both ``LOAD_INPUT`` and ``COMPUTE`` are finished with a latency of 4 instead of 1.

.. admonition:: Task - 3.7.1 Fix LOAD_INPUT latency issue
   :class: task
   
   To address this issue, uncomment ``#define EFFICIENT_IMPLEMENTATION`` in ``input_load.cpp``.

.. admonition:: Task - 3.7.2 Fix LOAD_WEIGHT latency issue
   :class: task

   Adapt the weight load in the ``weight_load.cpp`` file for efficient memory access.
   Hint! Take inspiration from ``input_load``


Use the same application ``task4`` located at ``/tutorial/model_hwpe/application/task4/test``.

.. admonition:: Verify - 3.7 Build and Execute
   :class: solution
   
   .. code-block:: bash
    
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary ./tutorial/model_hwpe/application/task4/test run --trace="hwpe"

If you have implemented the modification correctly, the output trace should resemble the following:

.. admonition:: Task - 3.7 Expected Trace
   :class: explanation
   
   .. code-block:: none
    
        2384905954: 171798: [/chip/cluster/hwpe/trace] ********************* First event enqueued *********************
        2384919836: 171799: [/chip/cluster/hwpe/trace] PRINTING CONFIGURATION REGISTER
        regconfig_manager >> INPUT POINTER : 0x1000001c
        regconfig_manager >> WEIGHT POINTER : 0x10000024
        regconfig_manager >> OUTPUT POINTER : 0x1000002c
        regconfig_manager >> WOFFS VALUE : 0xffffff80
        2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state START finished with latency : 0 cycles
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=0, latency=0, addr=28, data=0x44332211
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=0, latency=0, addr=32, data=0x88776655
        2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_INPUT finished with latency : 1 cycles
        2384933718: 171800: [/chip/cluster/hwpe/trace] (fsm state) current state WEIGHT_OFFSET finished with latency : 2 cycles
        2384961482: 171802: [/chip/cluster/hwpe/trace] weight load max_latency=0, latency=0, addr=0x24, data=0xccbbaa99
        2384961482: 171802: [/chip/cluster/hwpe/trace] weight load max_latency=0, latency=0, addr=0x28, data=0xfaffeadd
        2384961482: 171802: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_WEIGHT finished with latency : 1 cycles
        2384975364: 171803: [/chip/cluster/hwpe/trace] (fsm state) current state COMPUTE finished with latency : 1 cycles
        2384989246: 171804: [/chip/cluster/hwpe/trace] (fsm state) current state STORE_OUTPUT finished with latency : 1 cycles

In this trace, you will observe that there are only 2 load transactions performed for input and weight, and since there is no conflict, the latency is reduced to 0 for each transaction.
This optimization ensures that the operations complete efficiently without unnecessary delays.


