1. Getting Familiar with the HWPE model 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Let's have a look at the folder ``simple_hwpe``. It is very simple and useless at the moment, but we
will start building the Hwpe model by adding the necessary functional elements. The folder structure
looks like the following:

.. admonition:: Directory Structure

    .. code-block:: text
        
        /simple_hwpe
        ├── inc
        │   └── hwpe.hpp
        ├── src
        │   └── hwpe.cpp
        ├── CMakeLists.txt
        └── hwpe.py

    - **/simple_hwpe**: Code base for the simple HWPE.
    - **/inc/hwpe.hpp**: Instantiation of the Hwpe class.
    - **/src/hwpe.cpp**: Behavioral description of the Hwpe.
    - **CMakeLists.txt**: Compilation file requirements for the simple HWPE.
    - **hwpe.py**: Python generator to instantiate the Hwpe as used in the ``Cluster.py`` in Section 3.3.

.. admonition:: Task - 3.1.1 Getting familiar with hwpe.hpp file
   :class: task
   
   Have a look at the ``hwpe.hpp`` file and get familiar with it. 


Below is an example of a C++ class definition for Hwpe. This class inherits from ``vp::Component`` and includes public and private members along with a static method.   

.. code-block:: cpp

    class Hwpe : public vp::Component
    {
    public:
        Hwpe(vp::ComponentConf &config);
        vp::IoMaster tcdm_port;
        vp::Trace trace;
    private:
        vp::IoSlave cfg_port_;
        vp::WireMaster<bool> irq;
        static vp::IoReqStatus hwpe_slave(vp::Block *__this, vp::IoReq *req);
    };

- **Hwpe Class**: Definition of Hwpe class inherited from ``vp::Component``.
- **Hwpe(vp::ComponentConf &config)**: Constructor initializes the Hwpe with a configuration.
- **vp::IoMaster tcdm_port**: Master port for the TCDM access.
- **vp::Trace trace**: A trace object for logging and debugging.
- **vp::IoSlave cfg_port_**: I/O slave port for configuration.
- **static vp::IoReqStatus hwpe_slave(vp::Block *__this, vp::IoReq *req)**: A static method for handling I/O requests to the cfg port.

.. admonition:: Task - 3.1.2 Getting familiar with hwpe.cpp file
   :class: task
   
   Have a look at the ``hwpe.cpp`` file and get familiar with it. 

.. code-block:: cpp

    #include "hwpe.hpp"

    Hwpe::Hwpe(vp::ComponentConf &config)
        : vp::Component(config)
    {
        this->traces.new_trace("trace", &this->trace, vp::DEBUG);
        this->new_slave_port("config", &this->cfg_port_);
        this->new_master_port("irq", &this->irq);
        this->new_master_port("tcdm", &this->tcdm_port);
    }

    // The ‘hwpe_slave‘ member function models an access to the HWPE SLAVE interface
    vp::IoReqStatus Hwpe::hwpe_slave(vp::Block *__this, vp::IoReq *req)
    {
        Hwpe *_this = (Hwpe *)__this;
        _this->trace.msg("Received request (addr: 0x%x, size: 0x%x, is_write: %d, data: 0x%x\n", req->get_addr(), req->get_size(), req->get_is_write(), *(uint32_t *)(req->get_data()));
    }

    extern "C" vp::Component *gv_new(vp::ComponentConf &config)
    {
        return new Hwpe(config);
    }

- **#include "hwpe.hpp"**: Includes the header file for the Hwpe class.
- **Hwpe::Hwpe(vp::ComponentConf &config)**: Constructor initializes the Hwpe components:
  - It creates the master and slave ports and assigns them to the respective references.
  - A new trace object is created and referenced to the trace variable declared in the header.
- **hwpe_slave method**: Models an access to the HWPE slave interface.

