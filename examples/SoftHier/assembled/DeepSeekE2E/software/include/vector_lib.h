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
		asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(vlen));
		asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(va_addr));
		asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(vb_addr));
		asm volatile("vmslt.vv v0, v8, v16");
		asm volatile("vmv.v.v v24, v8");
		asm volatile("vmerge.vvm v8, v8, v16, v0");
		asm volatile("vmerge.vvm v16, v16, v24, v0");
		asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(va_addr));
		asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(vb_addr));
		vlen -= avl;
		va_addr += DATA_TYPE_BYTE*avl;
		vb_addr += DATA_TYPE_BYTE*avl;
	}
}

inline void vector_lib_compare_swap_with_idx(
	uint32_t va_addr,
	uint32_t ia_addr,
	uint32_t vb_addr,
	uint32_t ib_addr,
	uint32_t vlen)
{
	uint32_t avl;
	while(vlen > 0){
		asm volatile("vsetvli %0, %1, e" XSTR(DATA_TYPE_WIDTH) ", m8, ta, ma" : "=r"(avl) : "r"(vlen));
		asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(va_addr));
		asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(vb_addr));
		asm volatile("vmslt.vv v0, v8, v16");
		asm volatile("vmv.v.v v24, v8");
		asm volatile("vmerge.vvm v8, v8, v16, v0");
		asm volatile("vmerge.vvm v16, v16, v24, v0");
		asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(va_addr));
		asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(vb_addr));
		asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(ia_addr));
		asm volatile("vle" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(ib_addr));
		asm volatile("vmv.v.v v24, v8");
		asm volatile("vmerge.vvm v8, v8, v16, v0");
		asm volatile("vmerge.vvm v16, v16, v24, v0");
		asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v8,  (%0)" ::"r"(ia_addr));
		asm volatile("vse" XSTR(DATA_TYPE_WIDTH) ".v v16,  (%0)" ::"r"(ib_addr));
		vlen -= avl;
		if (vlen > 0)
		{
			va_addr += DATA_TYPE_BYTE*avl;
			ia_addr += DATA_TYPE_BYTE*avl;
			vb_addr += DATA_TYPE_BYTE*avl;
			ib_addr += DATA_TYPE_BYTE*avl;
		}
	}
}


void vector_lib_top_k(
	uint32_t val_addr,
	uint32_t idx_addr,
	uint32_t vlen,
	uint32_t row,
	uint32_t topk)
{
	topk = topk >= row? row - 1 : topk;
	for (int k = 0; k < topk; ++k)
	{
		for (int i = 0; i < (row - 1 - k); ++i)
		{
			uint32_t va_addr = val_addr + (row - 2 - i) * DATA_TYPE_BYTE * vlen;
			uint32_t ia_addr = idx_addr + (row - 2 - i) * DATA_TYPE_BYTE * vlen;
			uint32_t vb_addr = val_addr + (row - 1 - i) * DATA_TYPE_BYTE * vlen;
			uint32_t ib_addr = idx_addr + (row - 1 - i) * DATA_TYPE_BYTE * vlen;
			vector_lib_compare_swap_with_idx(va_addr, ia_addr, vb_addr, ib_addr, vlen);
		}
	}
}

void test_top_k(){
	uint32_t vlen = 8;
	uint32_t row = 8;
	uint32_t topk = 7;
	uint32_t val_addr = local(0);
	uint32_t idx_addr = val_addr + vlen * row * DATA_TYPE_BYTE;
	uint16_t *v_ptr = (uint16_t *) val_addr;
	uint16_t *i_ptr = (uint16_t *) idx_addr;

	printf("[Prepare]\n");
	for (int r = 0; r < row; ++r)
	{
		for (int l = 0; l < vlen; ++l)
		{
			int i = r*vlen + l;
			v_ptr[i] = (i*i) % 23;
			i_ptr[i] = r;
		}
	}

	printf("val:\n");
	for (int r = 0; r < row; ++r)
	{
		for (int l = 0; l < vlen; ++l)
		{
			int i = r*vlen + l;
			printf("%6d", v_ptr[i]);
		}
		printf("\n");
	}

	printf("idx:\n");
	for (int r = 0; r < row; ++r)
	{
		for (int l = 0; l < vlen; ++l)
		{
			int i = r*vlen + l;
			printf("%6d", i_ptr[i]);
		}
		printf("\n");
	}

	for (int k = 0; k < topk; ++k)
	{
		for (int i = 0; i < (row - 1 - k); ++i)
		{
			uint32_t va_addr = val_addr + (row - 2 - i) * DATA_TYPE_BYTE * vlen;
			uint32_t ia_addr = idx_addr + (row - 2 - i) * DATA_TYPE_BYTE * vlen;
			uint32_t vb_addr = val_addr + (row - 1 - i) * DATA_TYPE_BYTE * vlen;
			uint32_t ib_addr = idx_addr + (row - 1 - i) * DATA_TYPE_BYTE * vlen;
			vector_lib_compare_swap_with_idx(va_addr, ia_addr, vb_addr, ib_addr, vlen);
			printf("\n");
			printf("\n");
			printf("[Top K] k=%d, i=%d\n",k,i);
			printf("val:\n");
			for (int r = 0; r < row; ++r)
			{
				for (int l = 0; l < vlen; ++l)
				{
					int i = r*vlen + l;
					printf("%6d", v_ptr[i]);
				}
				printf("\n");
			}

			printf("idx:\n");
			for (int r = 0; r < row; ++r)
			{
				for (int l = 0; l < vlen; ++l)
				{
					int i = r*vlen + l;
					printf("%6d", i_ptr[i]);
				}
				printf("\n");
			}
		}
	}
}

#include "flex_transpose_engine.h"

void test_transposition()
{
	uint32_t vlen = 8;
	uint32_t row = 20;
	uint32_t val_addr = local(0);
	uint32_t idx_addr = val_addr + vlen * row * DATA_TYPE_BYTE;
	uint16_t *v_ptr = (uint16_t *) val_addr;
	uint16_t *i_ptr = (uint16_t *) idx_addr;

	printf("[Prepare]\n");
	for (int r = 0; r < row; ++r)
	{
		for (int l = 0; l < vlen; ++l)
		{
			int i = r*vlen + l;
			v_ptr[i] = r;
			i_ptr[i] = 0;
		}
	}

	printf("val:\n");
	for (int r = 0; r < row; ++r)
	{
		for (int l = 0; l < vlen; ++l)
		{
			int i = r*vlen + l;
			printf("%6d", v_ptr[i]);
		}
		printf("\n");
	}

	printf("idx:\n");
	for (int r = 0; r < vlen; ++r)
	{
		for (int l = 0; l < row; ++l)
		{
			int i = r*row + l;
			printf("%6d", i_ptr[i]);
		}
		printf("\n");
	}

	printf("Start Transposition\n");
	flex_transpose_engine_config(row,vlen,val_addr,idx_addr,DATA_TYPE_BYTE);
	flex_transpose_engine_trigger();
	flex_transpose_engine_wait();

	printf("val:\n");
	for (int r = 0; r < row; ++r)
	{
		for (int l = 0; l < vlen; ++l)
		{
			int i = r*vlen + l;
			printf("%6d", v_ptr[i]);
		}
		printf("\n");
	}

	printf("idx:\n");
	for (int r = 0; r < vlen; ++r)
	{
		for (int l = 0; l < row; ++l)
		{
			int i = r*row + l;
			printf("%6d", i_ptr[i]);
		}
		printf("\n");
	}
}

#endif