    .section .text
    .global _start
_start:
    // Stack initialization
    la    x2, stack

    // Exception handler
    la    t0, handle_exception
    csrw  0x305, t0

    // Do all other initializations from C code
    la    t0, __init_start
    addi  a0, x0, 0
    jalr  x1, t0


handle_exception:
    la    t0, exit
    li    a0, 1
    jr    t0

handle_exception_loop:
    j handle_exception_loop