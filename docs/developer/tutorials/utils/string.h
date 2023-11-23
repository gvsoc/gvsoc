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

#include <stddef.h>

void *memset(void *s, int c, size_t n);

void *memcpy(void *dst0, const void *src0, size_t len0);

int strcmp(const char *s1, const char *s2);

int strncmp(const char *s1, const char *s2, size_t n);

extern char  *strchr(const char *s, int c);

size_t strcspn( const char * s1, const char * s2 );

size_t strspn( const char * s1, const char * s2 );

extern void  *memmove(void *d, const void *s, size_t n);

extern char  *strcpy(char *d, const char *s);
extern char *strcat(char *dest, const char *src);

size_t strlen(const char *str);
