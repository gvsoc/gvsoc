6. Memory Load Operation - Weight Port
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Start the source setup by running:

.. admonition:: Task - 3.6.1 Setup task source files 
   :class: task
   
   .. code-block:: bash
        
        $ make model_hwpe_task5


.. admonition:: Task - 3.6.2 Complete the weight load operation  
   :class: task
   
   Implement the ``weight_load()`` function located in ``weight_load.cpp`` file. 
   Hint! Take inspiration from the ``input_load()`` function.

Once the implementation is complete verify the implementation, using the same test used in Task - 5.

.. admonition:: Verify - 3.6 Build and execute the application
   :class: solution
   
   .. code-block:: bash

       $ make build TARGETS=pulp-open-hwpe
       $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary ./docs/developer/tutorials/hwpe/model_hwpe/application/task4/test run --trace="hwpe"

If your implementation is correct, you will see the Traces 

.. admonition:: Task - 3.6 Expected Traces
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
        2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_INPUT finished with latency : 4 cycles
        2384975364: 171803: [/chip/cluster/hwpe/trace] (fsm state) current state WEIGHT_OFFSET finished with latency : 0 cycles
        2384975364: 171803: [/chip/cluster/hwpe/trace] weight load done addr=0x24, data=0x99
        2384975364: 171803: [/chip/cluster/hwpe/trace] weight load done addr=0x25, data=0xaa
        2384975364: 171803: [/chip/cluster/hwpe/trace] weight load done addr=0x26, data=0xbb
        2384975364: 171803: [/chip/cluster/hwpe/trace] weight load done addr=0x27, data=0xcc
        2384975364: 171803: [/chip/cluster/hwpe/trace] weight load done addr=0x28, data=0xdd
        2384975364: 171803: [/chip/cluster/hwpe/trace] weight load done addr=0x29, data=0xea
        2384975364: 171803: [/chip/cluster/hwpe/trace] weight load done addr=0x2a, data=0xff
        2384975364: 171803: [/chip/cluster/hwpe/trace] weight load done addr=0x2b, data=0xfa
        2384975364: 171803: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_WEIGHT finished with latency : 4 cycles
        2385030892: 171807: [/chip/cluster/hwpe/trace] (fsm state) current state COMPUTE finished with latency : 0 cycles
        2385030892: 171807: [/chip/cluster/hwpe/trace] (fsm state) current state STORE_OUTPUT finished with latency : 1 cycles

**Where are we?** 
We have successfully loaded the input and weights, but we haven't processed them yet. The next step involves building the missing datapath.
