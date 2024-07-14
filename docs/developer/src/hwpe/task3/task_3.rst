4. Introducing events in the HWPE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Start by setting up the source for this task:

.. admonition:: Task - 3.4.1 Setup task source files 
   :class: task
   
   .. code-block:: bash
        
        $ make model_hwpe_task3

Open ``hwpe.cpp`` file. Do you notice any difference compared to Task - 3.3?

In ``hwpe.cpp``, you will notice constructors for creating new events, such as ``fsm start event`` and ``fsm_event``, are already attached to their respective callback functions (``FsmStartHandler`` and ``FsmHandler``). However, the connection between ``fsm_end_event`` and ``FsmEndHandler`` is missing.

.. admonition:: Task - 3.4.2 Fix the missing connection 
   :class: task
   
   Create the connection between ``fsm_end_event`` and ``FsmEndHandler``.
   Hint: Use ``fsm start event`` and ``fsm_event`` as reference.

Next, let's handle the start of ``Hwpe`` execution. When ``HWPE_REG_COMMIT_AND_TRIGGER`` is set, the Hwpe execution should begin. Start by enqueueing the first event.

.. admonition:: Task - 3.4.3 Enqueue the first event 
   :class: task
   
   Enqueue an event with a latency of 1.

   .. code-block:: cpp
        
        _this->fsm_start_event->enqueue(1);
  
How do we verify when the event is executed? For simplicity add a Trace to the ``FsmStartHandler`` in ``hwpe_fsm.cpp``.


.. admonition:: Task - 3.4.4 Add trace to the event handler
   :class: task
   
   Add the following trace to the ``FsmStartHandler`` in ``hwpe_fsm.cpp``

   .. code-block:: cpp
        
        _this->trace.msg("Call back to FsmStartHandler\n");


.. admonition:: Verify - 3.4.4 Events
   :class: solution

   .. code-block:: bash
        
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary tutorial/model_hwpe/application/task3/test run --trace="hwpe"
   

.. admonition:: Task - 3.4.4 Expected traces
   :class: explanation

   .. code-block:: none
    
        2384905954: 171798: [/chip/cluster/hwpe/trace] Received request (addr: 0x0, size: 0x4, is_write: 1, data: 0x0)
        2384905954: 171798: [/chip/cluster/hwpe/trace] ********************* First event enqueued *********************
        2384919836: 171799: [/chip/cluster/hwpe/trace] Call back to FsmStartHandler

- The model received the configuration request at cycle 171798. In the same cycle it enqueues an event with a latency of 1 cycle.
- At cycle 171799, the event is executed from the clock engine, leading to the execution of the ``FsmStartHandler``.
- This results in the trace message "Call back to FsmStartHandler" at cycle 171799.

