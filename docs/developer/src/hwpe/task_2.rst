2 - Integrating a HWPE target to the hwpe-target  
................................................

Adding a HWPE target requires a functional model written in C++ and Python generators connected to the system. We will use the functional model located in the ``gvsoc/pulp/pulp/simple_hwpe`` directory. Our focus will be on integrating the existing simple hwpe into the newly created target ``hwpe-target``. Details of the simple hwpe will be discussed in Section XYZ. Let's begin the setup for this tutorial by copying the relevant source files for pulp open hwpe. These are the same files as before, with additional comments to guide you through the process.

.. admonition:: Task - 2.1 Setup 
   :class: task

   .. code-block:: bash
      
      $ cd gvsoc
      $ make integrate_hwpe_task1

All the necessary modifications are to be done in ``gvsoc/pulp/pulp/chips/pulp_open/cluster.json``. To include the new HWPE accelerator in the PULP system, an entry of the cluster needs to be updated in ``cluster.json``. The entry needs to be added below the ``dma`` entry as its base address comes next.

.. admonition:: Task - 2.1.1 Add HWPE entery to address map
   :class: task

   The base address 0x10201000 and a size of 0x400 is reserved for the registers of the new HWPE accelerator. The JSON entry is the following:

   .. code-block:: json

      "hwpe": {
          "mapping": {
              "base": "0x10201000",
              "size": "0x00000400",
              "remove_offset": "0x10200000"
          }
      },


After the ``hwpe`` is instantiated in ``cluster.json``, the hwpe model needs to be instantiated in the ``cluster.py`` file taking into account the architecture specific to the cluster. To instantiate the HWPE in the cluster, you have to first import the python generator of the ``hwpe.py`` located in the ``simple_hwpe`` folder.


.. admonition:: Task - 2.1.2 Instantiate HWPE in the Cluster
   :class: task

   Import the Hwpe Python wrapper into cluster.py. Use the import from NE16 as a reference:

   .. code-block:: python
        
        from pulp.simple_hwpe.hwpe import Hwpe

   The next step is to instantiate the Hwpe in the cluster.py. Again take inspiration from NE16

   .. code-block:: python
        
        hwpe = Hwpe(self, 'hwpe')


Now the HWPE instantiated in the cluster. However, there are no connections made to the other components in the cluster! First, we will start connecting the peripheral interconnect to the configuration port of the ``Hwpe`` This involves two steps. First, create an entry in the peripheral interconnect to accommodate the ``Hwpe`` This is where we can make use of the ``Hwpe`` entry in the ``cluster.json``. 

.. admonition:: Task - 2.1.3 Connection of Hwpe with the Peripheral interconnect
   :class: task

   Add the following line to create an entry to the peripheral interconnect with the port name of the Hwpe

   .. code-block:: python
        
        periph_ico.add_mapping('hwpe', **self._reloc_mapping(self.get_property('peripherals/hwpe/mapping')))


   The next part is to connect the peripheral interconnect's hwpe port to the config port of the Hwpe.

   .. code-block:: python
        
        self.bind(periph_ico, 'hwpe', hwpe, 'config')


In the previous steps, we added hwpe to the peripheral interconnect. Now let's add the port towards the TCDM. This also takes a similar approach to the peripheral interconnect. First, we need to add an additional port to the L1 subsystem. But this requires changes into the ``l1_subsystem.py`` file as follows:

.. admonition:: Task - 2.1.4 Adding dedicated port for Hwpe in the L1 subsystem
   :class: task

   Open the ``l1_subsystem.py`` and familiarise yourself. 

   .. code-block:: python
        
        l1_interleaver_nb_masters = nb_pe + 4 + 1 + 1
   
   Expose the added port as hwpe to outside using bind. Again take a hint from NE16

Next, we go back to the cluster.py file. The L1 subsystem is instantiated as l1. Thus, we connect the l1's port named hwpe to the Hwpe's port named ``tcdm``.

.. admonition:: Task - 2.1.4 Connection of Hwpe with L1 subsystem
   :class: task

   Connect the ``Hwpe``'s ``tcdm`` port to the ``l1``'s ``hwpe`` port

   .. code-block:: python
        
        self.bind(hwpe, 'tcdm', l1, 'hwpe')

The last part of the integration is to connect the event signal ``irq`` of the Hwpe to the cores.

.. admonition:: Task - 2.1.4 Connection of Hwpe with L1 subsystem
   :class: task

   Connect the Hwpe's ``irq`` port to the ``event_unit``'s ``hwpe_irq`` port

   .. code-block:: python
        
        hwpe_irq = self.get_property('pe/irq').index('acc_1')
        for i in range(0, nb_pe):
            self.bind(hwpe, 'irq', event_unit, 'in_event_%d_pe_%d' % (hwpe_irq, i))


.. admonition:: Verify - 2
   :class: solution
   
   .. code-block:: bash
      
      $ make build TARGETS=hwpe-target
      $ ./install/bin/gvsoc --target=hwpe-target --binary examples/pulp-open/hello image flash run


.. admonition:: Fixing failing build
   :class: task
   
   Search for hwpe in the gvsoc config.json file. What went wrong?
   Add the simple hwpe folder in gvsoc/pulp/pulp/CMakeLists.txt. Then rebuild the model and run the hello application as done previously.
