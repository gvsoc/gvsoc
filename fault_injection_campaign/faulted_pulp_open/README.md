# Running

- `make gvsoc` to compile the GVSOC with pulp-open target

- `make test` to run the program normally

- `make campaign` to run the campaign. The python script has to be run from `./..` as of now

# Overview

- Injects faults into L1/L2, prefetch buffer, register files. 

- Basic static executable analysis captures all ELF symbols.

- Everything stays within and is defined by `CampaignManager` object.

- Note that the executable has to be compiled with pulp-sdk or something. So do not delete it.

# TODO

All of the below are quality of life updates. The core is done.

- Write a more meaningful executable.
