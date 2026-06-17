# Running

- `make gvsoc` to compile the GVSOC with pulp-open target.

- `make test` to run the program normally (without faults).

- `make campaign` to run the fault campaign (script has to be run from `./..` as of now).

# Overview

- Injects faults into L1/L2 of `pulp_open`.

- Basic static executable analysis captures all ELF symbols.

- Uses `ficlib` suite.

# Prerequisites

FI capabilities have to be enabled in Python scripts inside `pulp` tree. To this end, in `pulp_open` script collection:
- Pass `fic_enabled=True` wherever memory is instantiated (`l1_subsystem.py` and `l2_subsystem.py`) because we target memories here
- Instantiate `fault_injection.fic` in `pulp_open/soc.py`
- Connect `FIC` to `axi_ico` in `pulp_open/soc.py` (`fic.o_GLOBAL_AS(axi_ico.i_INPUT())`) to enable access into global address space
