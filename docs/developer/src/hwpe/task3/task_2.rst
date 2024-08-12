3. Handle Configuration Requests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Next, we extend the ``Hwpe`` model to handle configuration registers, including special and general configuration registers. For simplicity, we have reduced the list of special registers for this tutorial. Here is an overview of the registers and their usage.

1. **HWPE_SOFT_CLEAR**: A SW-HR register for clearing HWPE states.
2. **HWPE_COMMIT_AND_TRIGGER**: A SW-HR register for initiating HWPE execution.
3. **HWPE_STATUS**: A SWR-HW register for checking HWPE execution status.

Additionally, we require four registers for HWPE functional execution, starting with:

1. **HWPE_REG_INPUT_PTR**: A SWR-HR register holding the input data pointer.
2. **HWPE_REG_WEIGHT_PTR** - A software-read-write hardware-read (SWR-HR) register that holds the pointer to the weights data.
3. **HWPE_REG_OUTPUT_PTR** - A software-read-write hardware-read (SWR-HR) register that holds the pointer to the output data.
4. **HWPE_REG_WEIGHT_OFFS** - A software-read-write hardware-read (SWR-HR) register that specifies the weight offset value.

.. admonition:: Task - 3.3 Getting familiar with the SW application
   :class: task
   
   Examine ``hal.h`` and ``test.c`` in ``tutorial/model_hwpe/application/task2``. What differences do you notice compared to task1?

We introduced special and configuration registers in ``hal.h``. In ``test.c``, we wrote a small application that clears and sets the configuration register. To handle this, we need to update the model. Currently, the register configuration function only prints the trace. In this task, we'll add functionality to the callback function ``hwpe_slave``.

.. admonition:: Task - 3.3.1 Setup task source files 
   :class: task
   
   .. code-block:: bash
        
        $ make model_hwpe_task2
        
    The Task - 3.3 source files are built on top of Task - 3.2. Did you notice any additional files compared to Task - 3.2?

The main difference is the addition of ``params.hpp`` and ``regconfig_manager.hpp`` in the ``inc`` folder. As the names suggest, ``params.hpp`` contains hardware-related parameters, similar to ``hal.h``. Similarly, ``regconfig_manager.hpp`` includes helper functions for register read and write operations, supporting modularity and potential future enhancements for debugging and control. Now we focus on handling the configuration requests. There are two tasks in this section.

The first task is to handle the software clear request in the ``hwpe.cpp`` file. Add a call to the ``clear()`` function at the appropriate placeholder. In the current stage ``clear()`` function which is empty. But we will fill the function in later part of the tutorial. Optionally you can also add a trace, and adding a trace would enable more debug info.

.. admonition:: Task - 3.3.2 Fix the configuration to handle clear command
   :class: task
   
   call the clear() function and add the following trace inside the ``clear()`` function.

   .. code-block:: cpp
        
        this->trace.msg("Hello from the clear function()\n");


.. admonition:: Task - 3.3.3 Assign the register content to the data
   :class: task
   
   .. code-block:: cpp
        
        *(uint32_t *) data = _this->regconfig_manager_instance.regfile_read((addr - HWPE_REGISTER_OFFS) >> 2);


It's time to build and verify the output. 

.. admonition:: Verify - 3.3 Handling Configuration
   :class: solution
   
   .. code-block:: bash
        
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary tutorial/model_hwpe/application/task2/test run --trace="hwpe"


If everything is implemented correctly, you should see in the traces the Hello message from ``clear()``.

.. admonition:: Task - 3.3 Expected Traces
   :class: explanation
   
   .. code-block:: none
    
        2349062630: 169216: [/chip/cluster/hwpe/trace                                     ] Received request (addr: 0x14, size: 0x4, is_write: 1, data: 0x0)
        2349062630: 169216: [/chip/cluster/hwpe/trace                                     ] Hello from the clear function()
