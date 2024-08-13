
6. Datapath
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Start the source setup by running:

.. admonition:: Task - 3.6.1 Setup task source files 
   :class: task
   
   .. code-block:: bash
        
        $ make model_hwpe_task6


We have integrated the compute part from ``neureka/inc/binconv`` and buffers from ``neureka/inc/buffer``. You can find the instantiation of these modules in ``hwpe.hpp`` and the includes in ``CMakeLists.txt``.
Let's see how to utilize the given datapath. 

The first task is to write data from the input streamer to the input buffer.

.. admonition:: Task - 3.6.2 Store data to input buffer 
   :class: task

    Open the ``input_load.cpp`` file. Write each 8-bit data to a single index in the buffer:

    .. code-block:: cpp
        
        this->input_buffer_.WriteAtIndex(this->input.iteration, 1, data[i]);

A similar approach is used for the weights. Once the data is loaded, it progresses through the weight offset phase. We utilize the partial sum obtained from the PE and multiply it with the offset factor. The offset phase takes ``WEIGHT_OFFSET_LATENCY`` cycles, which is set to 2. Computing can commence after this phase.


.. admonition:: Task - 3.6.3 Update weightoffset latency 
   :class: task
   
   Open ``hwpe_weightoffset.cpp`` and assign the latency
   
   .. code-block:: cpp
    
        latency = WEIGHT_OFFSET_LATENCY;

Next, we move on to the computation task. We need to accumulate the value from the output buffer.

.. admonition:: Task - 3.6.4 Read and Write to Output Buffer
   :class: task
    
    Open ``hwpe_compute.cpp`` and write the code to read the value from the accumulation buffer:
    
    .. code-block:: cpp
        
        OutputType sum = this->output_buffer_.ReadFromIndex(this->compute.iteration);
    
    After reading the value, perform partial sum computation using ``ComputePartialSum()`` of the PE instance. The next task is to write the data back to the output buffer.
    
    .. code-block:: cpp
        
        this->output_buffer_.WriteAtIndex(this->compute.iteration, 1, sum);


Use the same application ``task4`` located at ``/docs/developer/tutorials/hwpe/model_hwpe/application/task4/test`` to verify the implementation.

.. admonition:: Verify - 3.6 Build and execute the application
   :class: solution
   
   .. code-block:: bash
    
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary ./docs/developer/tutorials/hwpe/model_hwpe/application/task4/test run --trace="hwpe"

If your code is correct, the output trace should resemble the following:

.. admonition:: Task - 3.6 Expected Trace
   :class: explanation
   
   .. code-block:: none
    
        2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state START finished with latency : 0 cycles
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=0, latency=0, addr=0x1c, data=0x11
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=1, latency=1, addr=0x1d, data=0x22
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=2, latency=2, addr=0x1e, data=0x33
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=3, latency=3, addr=0x1f, data=0x44
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=3, latency=0, addr=0x20, data=0x55
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=3, latency=1, addr=0x21, data=0x66
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=3, latency=2, addr=0x22, data=0x77
        2384919836: 171799: [/chip/cluster/hwpe/trace] input load max_latency=3, latency=3, addr=0x23, data=0x88
        2384919836: 171799: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_INPUT finished with latency : 4 cycles
        2384975364: 171803: [/chip/cluster/hwpe/trace] (fsm state) current state WEIGHT_OFFSET finished with latency : 2 cycles
        2385003128: 171805: [/chip/cluster/hwpe/trace] (fsm state) current state LOAD_WEIGHT finished with latency : 4 cycles
        2385058656: 171809: [/chip/cluster/hwpe/trace] (fsm state) current state COMPUTE finished with latency : 1 cycles
        2385072538: 171810: [/chip/cluster/hwpe/trace] (fsm state) current state STORE_OUTPUT finished with latency : 1 cycles

We observe that the ``WEIGHT_OFFSET`` state is executed at cycle 171803 and finishes in 2 cycles. The next event, ``COMPUTE``, is reached 2 cycles later at 171805, as ``WEIGHT_OFFSET_LATENCY`` is set to 2.

.. admonition:: Task - 3.6.5 Reasoning
   :class: task
   
   Notice something unusual: the ``LOAD_INPUT`` state finishes in 4 cycles. Can you guess why this is happening? Look at the ``max_latency`` and ``latency`` factors for each load operation.
