#ifndef _VECTOR_LIB_H_
#define _VECTOR_LIB_H_

/*a < b? swap a b*/
void vector_lib_compare_swap(
	uint32_t va_addr,
	uint32_t vb_addr,
	uint32_t vlen)
{
	uint32_t avl;
	while(vlen > 0){
		asm volatile("vsetvli %0, %1, e16, m8, ta, ma" : "=r"(avl) : "r"(vlen));
		asm volatile("vle16.v v8,  (%0)" ::"r"(va_addr));
		asm volatile("vle16.v v16,  (%0)" ::"r"(vb_addr));
		asm volatile("vmflt.vv v0, v8, v16");
		asm volatile("vmv.v.v v24, v8");
		asm volatile("vmerge.vvm v8, v8, v16, v0");
		asm volatile("vmerge.vvm v16, v16, v24, v0");
		asm volatile("vse16.v v8,  (%0)" ::"r"(va_addr));
		asm volatile("vse16.v v16,  (%0)" ::"r"(vb_addr));
		vlen -= avl;
		va_addr += 2*avl;
		vb_addr += 2*avl;
	}
}

#endif