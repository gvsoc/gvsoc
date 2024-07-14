0 - Getting familiar with the file structure
............................................

.. admonition:: Directory Structure

    .. code-block:: text
      
      pulp/pulp
      ├── pulp-open.py
      ├── pulp
      │   ├── chips
      │   │   └── pulp_open
      │   │       ├── cluster.json
      │   │       ├── cluster.py
      │   │       ├── l1_subsystem.py
      │   │       ├── pulp_open.py
      │   │       └── pulp_open_board.py
      │   ├── neureka
      │   └── simple_hwpe
      │       ├── inc
      │       ├── src
      │       ├── CMakeLists.txt
      │       └── simple_hwpe.py

   - **/pulp**: Contains all the necessary source code for the tutorial.
   - **/pulp/pulp-open.py**: GVSoC target, similar to the chip name in the RTL context.
   - **/pulp/chips**: Different chips related setups.
   - **/pulp/chips/pulp-open/cluster.json**: Cluster configuration and memory maps.
   - **/pulp/chips/pulp-open/l1_subsystem.py**: Python generator for the L1 subsystem.
   - **/pulp/chips/pulp-open/cluster.py**: Main cluster wrapper connecting different components of the cluster.
   - **/pulp/neureka**: C++ model of hardware accelerator Neureka.
   - **/pulp/simple_hwpe**: C++ model of hardware accelerator simple_hwpe for today's tutorial.
   - **/pulp/simple_hwpe/inc**: Relevant includes.
   - **/pulp/simple_hwpe/src**: C++ source codes.
   - **/pulp/simple_hwpe/CMakeLists.txt**: Compilation file requirements.
   - **/pulp/simple_hwpe/simple_hwpe.py**: Python generator to instantiate the simple_hwpe.