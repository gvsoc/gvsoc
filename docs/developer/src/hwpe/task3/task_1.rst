2. SW execution including HWPE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
In this task, we execute a basic application on RISC-V to configure the HWPE model. The application files reside at ``/gvsoc/tutorial/model_hwpe/application/task1``. Here, the RISC-V cluster core writes ``0x12345678`` to the 0th configuration address via the ``cfg`` port, which is handled by the ``Hwpe::hwpe_slave(vp::Block * this, vp::IoReq *req)`` function.

.. admonition:: Verify - 3.2.1 Enable Traces
   :class: solution
   
   To enable tracing for HWPE and view prints, append --trace="hwpe"
   
   .. code-block:: bash
        
        $ make build TARGETS=pulp-open-hwpe
        $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary tutorial/model_hwpe/application/task1/test run --trace="hwpe"

Ideally, a trace related to Hwpe configuration should appear. If not, the issue may be in ``hwpe.cpp``, where there could be a missing link between the ``cfg`` port and the callback function. The model needs to associate the ``cfg`` port with the callback function ``vp::IoReqStatus Hwpe::hwpe_slave(vp::Block * this, vp::IoReq *req)``. Let's address this issue!

.. admonition:: Task - 3.2.2 Fix the trace issue
   :class: task

   Setup the source files with inline comments to guide solving the issue.
   
   .. code-block:: bash
        
        $ make model_hwpe_task1

   Add the following line to ``hwpe.cpp`` to establish the link:
   
   .. code-block:: cpp
        
        this->cfg_port_.set_req_meth(&Hwpe::hwpe_slave);


After making this change, rebuild the ``gvsoc`` model and run the application located in ``task1``.
If the code is implemented correctly, the trace should resemble the following:

.. admonition:: Task - 3.2.2 Expected Traces
   :class: explanation

   .. code-block:: none
        
        0: -1: [/chip/cluster/hwpe/comp] Creating final binding (/chip/cluster/hwpe:irq -> /chip/cluster/event_unit:in_event_13_pe_8)
        0: -1: [/chip/cluster/hwpe/comp] Creating final binding (/chip/cluster/hwpe:tcdm -> /chip/cluster/l1/interleaver:in_14)
        0: 0: [/chip/cluster/hwpe/comp] Reset (active: 1)
        0: 0: [/chip/cluster/hwpe/comp] Reset (active: 0)
        2380435950: 171476: [/chip/cluster/hwpe/trace] Received request (addr: 0x0, size: 0x4, is_write: 1, data: 0x12345678)
