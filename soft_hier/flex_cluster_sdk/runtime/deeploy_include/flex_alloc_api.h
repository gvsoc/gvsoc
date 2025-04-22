// flex_alloc_api.h
#ifndef FLEX_ALLOC_API_H
#define FLEX_ALLOC_API_H

#include <stdint.h>

void* flex_hbm_malloc(uint32_t size);
void  flex_hbm_free(void* ptr);

#endif