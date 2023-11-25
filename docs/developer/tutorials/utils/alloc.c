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

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "alloc.h"


#define PI_MEM_NB_ALLOCATORS 1

// Allocators for all memory regions
pi_alloc_t __mem_alloc_instances[PI_MEM_NB_ALLOCATORS];



// Allocate at least 4 bytes to avoid misaligned accesses when parsing free blocks
// and actually 8 to fit free chunk header size and make sure a e free block to always have
// at least the size of the header.
// This also requires the initial chunk to be correctly aligned.
#define MIN_CHUNK_SIZE 8

#define ALIGN_UP(addr,size)   (((addr) + (size) - 1) & ~((size) - 1))
#define ALIGN_DOWN(addr,size) ((addr) & ~((size) - 1))

#ifndef MAX
#define MAX(x, y) (((x)>(y))?(x):(y))
#endif





void __attribute__((noinline)) __mem_free(pi_alloc_t *a, void *_chunk, size_t size);


/*
  A semi general purpie memory allocator based on the assumption that when something is freed it's size is known.
  The rationnal is to get rid of the usual meta data overhead attached to traditionnal memory allocators.
*/

#if 0

void pi_alloc_dump(pi_alloc_t *a)
{
    pi_alloc_chunk_t *pt = a->first_free;

    printf("======== Memory allocator state: ============\n");
    for (pt = a->first_free; pt; pt = pt->next)
    {
        printf("Free Block at %8X, size: %8x, Next: %8X ", (unsigned int) pt, pt->size, (unsigned int) pt->next);
        if (pt == pt->next)
        {
            printf(" CORRUPTED\n"); break;
        }
        else
            printf("\n");
    }
    printf("=============================================\n");
}

#endif



void __mem_alloc_init(pi_alloc_t *a, void *_chunk, size_t size)
{
    pi_alloc_chunk_t *chunk = (pi_alloc_chunk_t *)ALIGN_UP((uintptr_t)_chunk, MIN_CHUNK_SIZE);
    a->first_free = chunk;
    size = size - ((uintptr_t)chunk - (uintptr_t)_chunk);
    if (size > 0)
    {
        chunk->size = ALIGN_DOWN(size, MIN_CHUNK_SIZE);
        chunk->next = NULL;
    }
}



void *__mem_alloc(pi_alloc_t *a, size_t size)
{
    pi_alloc_chunk_t *pt = a->first_free, *prev = 0;

    size = ALIGN_UP(size, MIN_CHUNK_SIZE);

    while (pt && (pt->size < size))
    {
        prev = pt; pt = pt->next;
    }

    if (pt)
    {
        if (pt->size == size)
        {
            // Special case where the whole block disappears
            // This special case is interesting to support when we allocate aligned pages, to limit fragmentation
            if (prev)
                prev->next = pt->next;
            else
                a->first_free = pt->next;
            return (void *)pt;
        }
        else
        {
            // The free block is bigger than needed
            // Return the beginning of the block to be contiguous with statically allocated data
            // and simplify memory power management
            void *result = (void *)((char *)pt);
            pi_alloc_chunk_t *new_pt = (pi_alloc_chunk_t *)((char *)pt + size);

            new_pt->size = pt->size - size;
            new_pt->next = pt->next;

            if (prev)
                prev->next = new_pt;
            else
                a->first_free = new_pt;

            return result;
        }
    }
    else
    {
        return NULL;
    }
}



void *__mem_alloc_align(pi_alloc_t *a, size_t size, size_t align)
{

    if (align < sizeof(pi_alloc_chunk_t))
        return __mem_alloc(a, size);

    // As the user must give back the size of the allocated chunk when freeing it, we must allocate
    // an aligned chunk with exactly the right size
    // To do so, we allocate a bigger chunk and we free what is before and what is after

    // We reserve enough space to free the remaining room before and after the aligned chunk
    int size_align = size + align + sizeof(pi_alloc_chunk_t) * 2;
    uintptr_t result = (uintptr_t)__mem_alloc(a, size_align);
    if (!result)
        return NULL;

    uintptr_t result_align = (result + align - 1) & -align;
    unsigned int headersize = result_align - result;

    // In case we don't get an aligned chunk at first, we must free the room before the first aligned one
    if (headersize != 0)
    {

        // If we don't have enough room before the aligned chunk for freeing the header, take the next aligned one
        if (result_align - result < sizeof(pi_alloc_chunk_t))
            result_align += align;

        // Free the header
        __mem_free(a, (void *)result, headersize);
    }

    // Now free what remains after
    __mem_free(a, (unsigned char *)(result_align + size), size_align - headersize - size);

    return (void *)result_align;
}



void __mem_free(pi_alloc_t *a, void *_chunk, size_t size)
{
    pi_alloc_chunk_t *chunk = (pi_alloc_chunk_t *)_chunk;
    pi_alloc_chunk_t *next = a->first_free, *prev = 0, *new;
    size = ALIGN_UP(size, MIN_CHUNK_SIZE);

    while (next && next < chunk)
    {
        prev = next; next = next->next;
    }

    if (((char *)chunk + size) == (char *)next)
    {
        /* Coalesce with next */
        chunk->size = size + next->size;
        chunk->next = next->next;
    }
    else
    {
        chunk->size = size;
        chunk->next = next;
    }

    if (prev)
    {
        if (((char *)prev + prev->size) == (char *)chunk)
        {
            /* Coalesce with previous */
            prev->size += chunk->size;
            prev->next = chunk->next;
        }
        else
        {
            prev->next = chunk;
        }
    }
    else
    {
        a->first_free = chunk;
    }
}


extern unsigned char __mem_end;


void __mem_alloc_init_all()
{
    __mem_alloc_init(&__mem_alloc_instances[0],
        &__mem_end, 0x00100000 - (size_t)&__mem_end);
}