import re
import argparse

tasks = {
    1: {
        "patterns": [
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x0, size: 0x4, is_write: 1, data: 0x12345678\)"
        ],
        "repetitions": [0],
        "offsets": [0]
    },
    2: {
        "patterns": [
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x14, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Hello from the clear function\(\)"
        ],
        "repetitions": [1],
        "offsets": [0]
    },
    3: {
        "patterns": [
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x14, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x20, size: 0x4, is_write: 1, data: 0x1000001c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 0 data: 1000001c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x24, size: 0x4, is_write: 1, data: 0x10000024\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 1 data: 10000024",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x28, size: 0x4, is_write: 1, data: 0x1000002c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 2 data: 1000002c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x2c, size: 0x4, is_write: 1, data: 0xffffff80\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 3 data: ffffff80",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x0, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\* First event enqueued \*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Call back to FsmStartHandler"
        ],
        "repetitions": [2],
        "offsets": [1]
    },
    4: {
        "patterns": [
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x0, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \*{21} First event enqueued \*{21}",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] PRINTING CONFIGURATION REGISTER",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state START finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x1c, data=0x11",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x1d, data=0x22",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x1e, data=0x33",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x1f, data=0x44",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x20, data=0x55",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x21, data=0x66",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x22, data=0x77",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x23, data=0x88",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] iteration=8, count=8",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_INPUT finished with latency : 4 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state WEIGHT_OFFSET finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_WEIGHT finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state COMPUTE finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state STORE_OUTPUT finished with latency : 1 cycles"
        ],
        "repetitions": [3, 12, 2],
        "offsets": [0, 4, 5]
    },
    5: {
        "patterns": [
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x14, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x20, size: 0x4, is_write: 1, data: 0x1000001c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 0 data: 1000001c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x24, size: 0x4, is_write: 1, data: 0x10000024\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 1 data: 10000024",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x28, size: 0x4, is_write: 1, data: 0x1000002c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 2 data: 1000002c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x2c, size: 0x4, is_write: 1, data: 0xffffff80\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 3 data: ffffff80",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x0, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \*{21} First event enqueued \*{21}",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] PRINTING CONFIGURATION REGISTER",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state START finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x1c, data=0x11",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x1d, data=0x22",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x1e, data=0x33",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x1f, data=0x44",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x20, data=0x55",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x21, data=0x66",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x22, data=0x77",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Input load for addr=0x23, data=0x88",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_INPUT finished with latency : 4 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state WEIGHT_OFFSET finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load done  addr=0x24, data=0x99",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load done  addr=0x25, data=0xaa",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load done  addr=0x26, data=0xbb",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load done  addr=0x27, data=0xcc",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load done  addr=0x28, data=0xdd",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load done  addr=0x29, data=0xea",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load done  addr=0x2a, data=0xff",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load done  addr=0x2b, data=0xfa",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_WEIGHT finished with latency : 4 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state COMPUTE finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state STORE_OUTPUT finished with latency : 1 cycles"
        ],
        "repetitions": [1, 10, 10],
        "offsets": [0, 4, 8]
    },
    6: {
        "patterns": [
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x14, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x20, size: 0x4, is_write: 1, data: 0x1000001c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 0 data: 1000001c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x24, size: 0x4, is_write: 1, data: 0x10000024\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 1 data: 10000024",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x28, size: 0x4, is_write: 1, data: 0x1000002c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 2 data: 1000002c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x2c, size: 0x4, is_write: 1, data: 0xffffff80\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 3 data: ffffff80",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x0, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \*{21} First event enqueued \*{21}",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] PRINTING CONFIGURATION REGISTER",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state START finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=0, latency=0, addr=0x1c, data=0x11",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=1, latency=1, addr=0x1d, data=0x22",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=2, latency=2, addr=0x1e, data=0x33",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=3, latency=3, addr=0x1f, data=0x44",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=3, latency=0, addr=0x20, data=0x55",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=3, latency=1, addr=0x21, data=0x66",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=3, latency=2, addr=0x22, data=0x77",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=3, latency=3, addr=0x23, data=0x88",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_INPUT finished with latency : 4 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state WEIGHT_OFFSET finished with latency : 2 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_WEIGHT finished with latency : 4 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state COMPUTE finished with latency : 1 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state STORE_OUTPUT finished with latency : 1 cycles"
        ],
        "repetitions": [1, 1, 1, 11, 2],
        "offsets": [1, 5, 7, 11, 12]
    },
    7: {
        "patterns": [
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x14, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x20, size: 0x4, is_write: 1, data: 0x1000001c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 0 data: 1000001c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x24, size: 0x4, is_write: 1, data: 0x10000024\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 1 data: 10000024",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x28, size: 0x4, is_write: 1, data: 0x1000002c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 2 data: 1000002c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x2c, size: 0x4, is_write: 1, data: 0xffffff80\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 3 data: ffffff80",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x0, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \*{21} First event enqueued \*{21}",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] PRINTING CONFIGURATION REGISTER",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state START finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=0, latency=0, addr=0x1c, data=0x44332211",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=0, latency=0, addr=0x20, data=0x88776655",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_INPUT finished with latency : 1 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state WEIGHT_OFFSET finished with latency : 2 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load max_latency=0, latency=0, addr=0x24, data=0xccbbaa99",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load max_latency=0, latency=0, addr=0x28, data=0xfaffeadd",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_WEIGHT finished with latency : 1 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state COMPUTE finished with latency : 1 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state STORE_OUTPUT finished with latency : 1 cycles"
        ],
        "repetitions": [1, 3, 1, 5, 2],
        "offsets": [1, 2, 4, 5, 6]
    },
    8: {
        "patterns": [
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x14, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x20, size: 0x4, is_write: 1, data: 0x1000001c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 0 data: 1000001c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x24, size: 0x4, is_write: 1, data: 0x10000024\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 1 data: 10000024",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x28, size: 0x4, is_write: 1, data: 0x1000002c\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 2 data: 1000002c",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x2c, size: 0x4, is_write: 1, data: 0xffffff80\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Setting Register offset: 3 data: ffffff80",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] Received request \(addr: 0x0, size: 0x4, is_write: 1, data: 0x0\)",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \*{21} First event enqueued \*{21}",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] PRINTING CONFIGURATION REGISTER",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state START finished with latency : 0 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=0, latency=0, addr=0x1c, data=0x966898fc",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] input load max_latency=0, latency=0, addr=0x20, data=0x921cf02b",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_INPUT finished with latency : 1 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state WEIGHT_OFFSET finished with latency : 2 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load max_latency=0, latency=0, addr=0x24, data=0xd52a30fb",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] weight load max_latency=0, latency=0, addr=0x28, data=0xc3119a1d",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state LOAD_WEIGHT finished with latency : 1 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state COMPUTE finished with latency : 1 cycles",
            r"(\d+): (\d+): \[\x1b\[34m/chip/cluster/hwpe/trace\s+\x1b\[0m\] \(fsm state\) current state STORE_OUTPUT finished with latency : 1 cycles"
        ],
        "repetitions": [1, 3, 1, 5, 2],
        "offsets": [1, 2, 4, 5, 6]
    },
}

VERBOSE = 0

def check_trace_task(trace_file, task_number, patterns, repetitions, offsets):
    if VERBOSE:
        print(f"Checking Task {task_number} in {trace_file}")
        print(f"Patterns: {patterns}")

    with open(trace_file, 'r') as file:
        lines = file.readlines()

    matches = []
    for pattern in patterns:
        for line in lines:
            match = re.search(pattern, line)
            if match:
                matches.append((match.group(1), match.group(2), line.strip()))
                break

    if len(matches) != len(patterns):
        print(f"Error: Not all expected patterns were found in the trace file for Task {task_number}. Expected = {len(patterns)}, Actual = {len(matches)}")
        return False

    if VERBOSE:
        print(f"Found relevant lines in Task {task_number}:")
        for match in matches:
            print(f"Line: {match[2]}")

    # Check the difference between consecutive lines they should be in increasing order
    for i in range(1, len(matches)):
        current_number = int(matches[i][1])
        previous_number = int(matches[i-1][1])

        if current_number < previous_number:
            print(f"Error: Cycles is inconsistent between lines {i} and {i+1}. Previous number: {previous_number}, Current number: {current_number}")
            return False

    end_number = int(matches[-1][1])
    sum_repetitions = 0
    for i, repeat_val in enumerate(repetitions):
        for j in range(repeat_val):
            index = -(j + 2 + sum_repetitions)
            current_number = int(matches[index][1])
            if end_number - current_number != offsets[i]:
                print(f"Error in line {index} in the trace. Expected {end_number - offsets[i]} cycles, got {current_number}")
                return False
        sum_repetitions += repeat_val

    print(f"Success: All checks passed for Task {task_number}.")
    return True

if __name__ == "__main__":

    parser      = argparse.ArgumentParser(description="Check the trace files for specific patterns.")
    parser.add_argument("task_number", type=int, choices=range(1, 9), help="The task number to check (1-8).")
    
    args        = parser.parse_args()
    task_number = args.task_number
    task_data   = tasks[task_number]
    task_file   = f"./traces/model_hwpe_task{task_number}.txt"
    task_result = check_trace_task(task_file, task_number, task_data["patterns"], task_data["repetitions"], task_data["offsets"])

    if task_result:
        print(f"Task {task_number} executed successfully")
    else:
        print("One or more checks failed.")
        exit(1)
