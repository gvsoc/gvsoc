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

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "semihost.h"
#include "io.h"



#define LIBC_PUTC_BUFFER_SIZE 128

static char __libc_buffer[LIBC_PUTC_BUFFER_SIZE];
static int __libc_buffer_index = 0;



int __libc_fputc_safe(int c, FILE *stream)
{
    char *buffer = __libc_buffer;
    int *index = &__libc_buffer_index;

    buffer[*index] = c;
    *index = *index + 1;

    if (*index == LIBC_PUTC_BUFFER_SIZE || c == '\n')
    {
        buffer[*index] = 0;

        __libc_semihost_write(1, buffer, *index);
        *index = 0;
    }

    return 0;
}




int puts(const char *s)
{
    char c;
    do
    {
        c = *s;
        if (c == 0)
        {
            __libc_fputc_safe('\n', NULL);
            break;
        }
        __libc_fputc_safe(c, NULL);
        s++;
    } while(1);

    return 0;
}



int fputc(int c, FILE *stream)
{
    __libc_fputc_safe(c, NULL);

    return 0;
}



int putchar(int c)
{
    return fputc(c, stdout);
}



int __libc_prf(int (*func)(), void *dest, const char *format, va_list vargs)
{
    return __libc_prf_safe(func, dest, format, vargs);
}



void exit(int status)
{
    __libc_semihost_exit(status == 0 ? SEMIHOST_EXIT_SUCCESS : SEMIHOST_EXIT_ERROR);

    while(1);
}



void abort()
{
    exit(-1);
}
