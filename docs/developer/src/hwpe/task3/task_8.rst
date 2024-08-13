8. The final step - FSM termination
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. admonition:: Task - 3.8.1 Setup task source files 
   :class: task
   
   .. code-block:: bash
        
        $ make model_hwpe_task8
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary ./docs/developer/tutorials/hwpe/model_hwpe/application/task8/test run --trace="hwpe"


**Warning! This command will hang because the core will be stuck at ``HWPE_WRITE_CMD(HWPE_COMMIT_AND_TRIGGER, HWPE_TRIGGER_CMD)``.**
To resolve this issue, we should use the ``irq`` port connected to the event unit.

.. admonition:: Task - 3.8.2 Fix FsmEndHandler
   :class: task
   
   Open ``hwpe_fsm.cpp`` and in ``FsmEndHandler``, uncomment the line ``irq.sync(true)``.


Use the ``task8`` application located at ``/docs/developer/tutorials/hwpe/model_hwpe/application/task8/test``.

Build and execute the application:

.. admonition:: Verify - 3.8 Build and execute the application
   :class: solution
   
   .. code-block:: bash
    
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary ./tutorial/model_hwpe/application/task8/test run --trace="hwpe"

If implemented correctly, the output trace should resemble the following:

.. admonition:: Task - 3.8 Expected Trace
   :class: explanation
   
   .. code-block:: none
        
        2388598566: 172064: [/chip/cluster/hwpe/trace] ********************* First event enqueued *********************
        2388612448: 172065: [/chip/cluster/hwpe/trace] PRINTING CONFIGURATION REGISTER
        regconfig_manager >> INPUT POINTER : 0x1000001c
        regconfig_manager >> WEIGHT POINTER : 0x10000024
        regconfig_manager >> OUTPUT POINTER : 0x1000002c
        regconfig_manager >> WOFFS VALUE : 0xffffff80
        2388612448: 172065: [/chip/cluster/hwpe/trace] (fsm state) current state START finished with latency : 0 cycles
        2388612448: 172065: [/chip/cluster/hwpe/trace] input load max_latency=0, latency=0, addr=28, data=0x966898fc
        2388612448: 172065: [/chip/cluster/hwpe/trace] input load max_latency=0, latency=0, addr=32, data=0x921cf02b
        2388612448: 172065: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_INPUT finished with latency : 1 cycles
        2388626330: 172066: [/chip/cluster/hwpe/trace] (fsm state) current state WEIGHT_OFFSET finished with latency : 2 cycles
        2388654094: 172068: [/chip/cluster/hwpe/trace] input load max_latency=0, latency=0, addr=0x24, data=0xd52a30fb
        2388654094: 172068: [/chip/cluster/hwpe/trace] input load max_latency=0, latency=0, addr=0x28, data=0xc3119a1d
        2388654094: 172068: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_WEIGHT finished with latency : 1 cycles
        2388667976: 172069: [/chip/cluster/hwpe/trace] (fsm state) current state COMPUTE finished with latency : 1 cycles
        2388681858: 172070: [/chip/cluster/hwpe/trace] (fsm state) current state STORE_OUTPUT finished with latency : 1 cycles
        2389487014: 172128: [/chip/cluster/hwpe/trace] Received request (addr: 0xc, size: 0x4, is_write: 0, data: 0x0)

In this trace, note the successful completion of various states and operations, culminating in the ``Received request`` indicating the end of accelerator execution.

This setup ensures proper termination handling and synchronization using the irq port.