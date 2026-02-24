import os
import numpy as np
import preload as pld

rng = np.random.default_rng()
A=rng.random((64, 64)).astype(np.float16)
B=rng.random((64, 64)).astype(np.float16)
C=rng.random((64, 64)).astype(np.float16)
D=rng.random((64, 64)).astype(np.float16)

matrix_size_in_byte = A.size * 2
HBM_base_address = 0xc0000000

preload_A_into_HBM_addr = HBM_base_address + 0
preload_B_into_HBM_addr = HBM_base_address + matrix_size_in_byte
preload_C_into_HBM_addr = HBM_base_address + matrix_size_in_byte * 2
preload_D_into_HBM_addr = HBM_base_address + matrix_size_in_byte * 3

pld.make_preload_elf(
	"my_preload.elf",
	[A,B,C,D],
	[
	preload_A_into_HBM_addr,
	preload_B_into_HBM_addr, 
	preload_C_into_HBM_addr, 
	preload_D_into_HBM_addr
	])