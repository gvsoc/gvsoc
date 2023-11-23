Tutorials
---------



How to build a system from scratch
..................................

How to write a component from scratch
...................................

Add reset

How to make components communicate together
...........................................

How to add system traces to a component
.......................................

How to add VCD traces to a component
....................................

How to customize gtkwave view
.............................

How to add a register map in a component
........................................

How to add timing
.................
Simple sync reuests with timing.
Asynchronous ones with events.

How to use the IO request interface
...................................

Synchronous and asychronous requests.

How to add mutiple cores
........................

How to handle clock domains and frequency scaling
.................................................

How to customize an interconnect timing
.......................................

How to add an ISS instruction
.............................

static inline iss_reg_t my_instr_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, REG_GET(0) + 2*REG_GET(1));
    return iss_insn_next(iss, insn, pc);
}



R5('my_instr', 'R',  '0000010 ----- ----- 110 ----- 0110011'),

How to time an ISS instruction
..............................
Multi-cycles instructions, fixed or dynamic latency








How to add an ISS isa
.....................
How to add power traces to a component
......................................
How to build a multi-chip system
................................

How to control GVSOC from an external simulator
...............................................

How to control GVSOC from a python script
.........................................
