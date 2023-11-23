GVSOC architecture
==================

Overview
........

.. image:: images/gvsoc_archi.png

.. image:: images/gvsoc_proxy_archi.png


Code organization
.................



| core/engine
| ├── CMakeLists.txt
| ├── include
| │   ├── gv
| │   │   ├── gvsoc.hpp
| │   │   └── testbench.hpp
| │   └── vp
| │       ├── block.hpp
| │       ├── component.hpp
| │       ├── jsmn.h
| │       ├── json.hpp
| │       ├── launcher.hpp
| │       ├── ports.hpp
| │       ├── proxy_client.hpp
| │       ├── proxy.hpp
| │       ├── queue.hpp
| │       ├── register.hpp
| │       ├── signal.hpp
| │       ├── top.hpp
| │       ├── vp.hpp
| │       ├── time
| │       │   ├── block_time.hpp
| │       │   ├── implementation.hpp
| │       │   ├── time_engine.hpp
| │       │   └── time_event.hpp
| │       ├── clock
| │       │   ├── block_clock.hpp
| │       │   ├── clock_engine.hpp
| │       │   ├── clock_event.hpp
| │       │   └── implementation.hpp
| │       ├── power
| │       │   ├── block_power.hpp
| │       │   ├── power.hpp
| │       │   ├── power_source.hpp
| │       │   ├── power_table.hpp
| │       │   └── power_trace.hpp
| │       ├── trace
| │       │   ├── block_trace.hpp
| │       │   ├── event_dumper.hpp
| │       │   ├── implementation.hpp
| │       │   ├── trace_engine.hpp
| │       │   └── trace.hpp
| │       ├── gdbserver
| │       │   └── gdbserver_engine.hpp
| │       └── itf
| │           ├── clk.hpp
| │           ├── clock.hpp
| │           ├── cpi.hpp
| │           ├── hyper.hpp
| │           ├── i2c.hpp
| │           ├── i2s.hpp
| │           ├── implem
| │           │   ├── clock_class.hpp
| │           │   ├── clock.hpp
| │           │   ├── wire_class.hpp
| │           │   └── wire.hpp
| │           ├── io.hpp
| │           ├── jtag.hpp
| │           ├── qspim.hpp
| │           ├── uart.hpp
| │           ├── wire.hpp
| │           └── wire.json
| └── src
|     ├── block.cpp
|     ├── component.cpp
|     ├── jsmn.cpp
|     ├── json.cpp
|     ├── launcher.cpp
|     ├── main.cpp
|     ├── main_systemc.cpp
|     ├── main_systemc.hpp
|     ├── ports.cpp
|     ├── proxy_client.cpp
|     ├── proxy.cpp
|     ├── queue.cpp
|     ├── register.cpp
|     ├── signal.cpp
|     ├── top.cpp
|     ├── time
|     │   ├── block_time.cpp
|     │   ├── time_engine.cpp
|     │   └── time_event.cpp
|     ├── clock
|     │   ├── block_clock.cpp
|     │   ├── clock_engine.cpp
|     │   └── clock_event.cpp
|     ├── power
|     │   ├── block_power.cpp
|     │   ├── power_engine.cpp
|     │   ├── power_source.cpp
|     │   ├── power_table.cpp
|     │   └── power_trace.cpp
|     └── trace
|         ├── event.cpp
|         ├── fst
|         │   ├── block_format.txt
|         │   ├── fastlz.c
|         │   ├── fastlz.h
|         │   ├── fstapi.c
|         │   ├── fstapi.h
|         │   ├── lz4.c
|         │   ├── lz4.h
|         │   ├── Makefile.am
|         │   └── Makefile.in
|         ├── fst.cpp
|         ├── lxt2.cpp
|         ├── lxt2_write.c
|         ├── lxt2_write.h
|         ├── raw
|         │   ├── trace_dumper.cpp
|         │   ├── trace_dumper_example.cpp
|         │   ├── trace_dumper.hpp
|         │   ├── trace_dumper_types.h
|         │   └── trace_dumper_utils.h
|         ├── raw.cpp
|         ├── trace.cpp
|         ├── trace_domain_impl.cpp
|         ├── vcd.cpp
|         └── wavealloca.h
