/*
 * Copyright (C) 2023 GreenWaves Technologies, ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once


// This is the header of every free block. It is used to chain free blocks together
typedef struct pi_alloc_block_s
{
    // Size of the free block
    int                      size;
    // Pointer to the next free block.
    struct pi_alloc_block_s *next;
}
pi_alloc_chunk_t;



// Structure for an allocator
typedef struct
{
    // First free block
    pi_alloc_chunk_t *first_free;
} pi_alloc_t;


extern pi_alloc_t __mem_alloc_instances[];


void __mem_alloc_init_all();
void __mem_alloc_init(pi_alloc_t *a, void *_chunk, size_t size);
void *__mem_alloc_align(pi_alloc_t *a, size_t size, size_t align);
void __mem_free(pi_alloc_t *a, void *_chunk, size_t size);
void *__mem_alloc(pi_alloc_t *a, size_t size);



static inline void *pi_mem_alloc(int allocator, size_t size)
{
    // Just forward to the right accelerator
    void *chunk = __mem_alloc(&__mem_alloc_instances[allocator], size);
    return chunk;
}



static inline void pi_mem_free(int allocator, void *data, size_t size)
{
    // Just forward to the right accelerator
    __mem_free(&__mem_alloc_instances[allocator], data, size);
}


static inline void *pi_malloc(size_t size)
{
    return pi_mem_alloc(0, size);
}

static inline void pi_free(void *chunk, size_t size)
{
    return pi_mem_free(0, chunk, size);
}