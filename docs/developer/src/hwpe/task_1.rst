1 - Create a new target on the top of pulp-open
................................................
In this section, you will create a new Hardware Target ``pulp-open-hwpe`` which is a replica of the pulp-open consisting of a pulp-cluster. Later we will develop on the ``pulp-open-hwpe`` to add our accelerator. 

Please go through the README to install gvsoc. 

After the installation is done, Setup the source files to add inline comments to help in performing the tasks.

.. admonition:: Task - 1.0 setup the source files  
   :class: task

   .. code-block:: bash

      $ make create_target_task1

Let's create a new gvsoc target ``pulp-open-hwpe`` at ``gvsoc/pulp/`` by copying the pulp-open target.

.. admonition:: Task - 1.1 Create pulp-open-hwpe 
   :class: task

   .. code-block:: bash

      $ cd gvsoc/pulp
      $ cp pulp-open.py pulp-open-hwpe.py


The new target should work like the pulp-open because it is a copy of the pulp-open. To verify the working, build the gvsoc using TARGET=pulp-open-hwpe and run the same hello binary with this target.

.. admonition:: Verify - 1.1 
   :class: solution
   
   .. code-block:: bash
      
      $ make build TARGETS=pulp-open-hwpe
      $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary examples/pulp-open/hello image flash run

You should see the Hello code passing successfully. 

.. admonition:: Task-1.2.1 Familiarize the contents of pulp-open-hwpe.py
   :class: task
   
   Open the newly created ``pulp-open-hwpe.py`` and familiarise yourself. What are your observation?

It is a `GAPY_TARGET`. It relies on the imports from the `pulp/chips/pulp_open` folder. Next we will create a dedicated SoC with a pulp-cluster using the pulp-open template. 

.. code-block:: python
    
    from pulp.chips.pulp_open.pulp_open_board import Pulp_open_board
    import gvsoc.runner as gvsoc

In the next task create a dedicated folder by copying the the contents of the pulp_open to pulp_open_hwpe. Then in the later exercises we will change the contents of the pulp_open_hwpe folder.

.. admonition:: Task-1.2.2 Create a replica of pulp_open_board
   :class: task

   .. code-block:: bash

      $ cd gvsoc/pulp/pulp/chips
      $ mkdir pulp_open_hwpe 
      $ cp pulp_open/* pulp_open_hwpe -r

Even though we created new pulp_open_hwpe folder, the pulp-open-hwpe.py still points to the pulp_open folder. 
The next part is to change the dependencies to point to the new pulp_open_hwpe files by replacing the correcting path for the model imports. 
 
.. admonition:: Task-1.2.3 Fix the dependencies for pulp-open-hwpe
   :class: task
   
   .. code-block:: python
      
      from pulp.chips.pulp_open_hwpe.pulp_open_board import Pulp_open_board
      import gvsoc.runner as gvsoc

   Similiarly, the references in the following files needs to be modified. Look for the inline hints. 
   
   .. code-block:: text

       /gvsoc/pulp/pulp/chips/pulp_open_hwpe
       ├── pulp_open_board.py
       ├── pulp_open.py
       └── cluster.py

After modifications, you can verify that the changes are correct by building GVSoC with the new target
and running the hello application again by executing the following commands:

.. admonition:: Verify - 1.2.3 
   :class: solution
   
   .. code-block:: bash
      
      $ make build TARGETS=pulp-open-hwpe
      $ ./install/bin/gvsoc --target=pulp-open-hwpe --binary examples/pulp-open/hello image flash run

The test should pass without any issue. How do you know if your changes are reflected correctly?

.. admonition:: Information
   :class: explanation
   
   GVSoC generates a ``gvsoc_config.json`` file in the ``/gvsoc`` folder when an application is executed. This is a tool generated file and you can find all the address maps as well as the component connections. Now we can see the changes such as ``cluster_config_file: pulp/chips/pulp-open-hwpe/cluster.json`` in the generated ``gvsoc_config.json`` file.