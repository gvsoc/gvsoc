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


/*
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */


#include <stddef.h>
#include <stdint.h>



static int errno;
int *__errno() { return &errno; }



int strcmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && *s1 == *s2)
    {
        s1++;
        s2++;
    }

    return (*(unsigned char *) s1) - (*(unsigned char *) s2);
}



int strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0)
        return 0;

    while (n-- != 0 && *s1 == *s2)
    {
        if (n == 0 || *s1 == '\0')
            break;
        s1++;
        s2++;
    }

    return (*(unsigned char *) s1) - (*(unsigned char *) s2);
}



size_t strlen(const char *str)
{
    const char *start = str;

    while (*str)
        str++;
    return str - start;
}



int memcmp(const void *m1, const void *m2, size_t n)
{
    unsigned char *s1 = (unsigned char *) m1;
    unsigned char *s2 = (unsigned char *) m2;

    while (n--)
    {
        if (*s1 != *s2)
        {
            return *s1 - *s2;
        }
        s1++;
        s2++;
    }
    return 0;
}



void *memset(void *m, int c, size_t n)
{
    char *s = (char *)m;
    while (n--)
        *s++ = (char) c;

    return m;
}



void *memcpy(void *dst0, const void *src0, size_t len0)
{
    void *save = dst0;

    char src_aligned = (((size_t) src0) & 3) == 0;
    char dst_aligned = (((size_t) dst0) & 3) == 0;
    char copy_full_words = (len0 & 3) == 0;

    if (src_aligned && dst_aligned && copy_full_words)
    {
        // all accesses are aligned => can copy full words
        uint32_t *dst = (uint32_t *) dst0;
        uint32_t *src = (uint32_t *) src0;

        while (len0)
        {
            *dst++ = *src++;
            len0 -= 4;
        }
    }
    else
    {
        // copy bytewise
        char *dst = (char *) dst0;
        char *src = (char *) src0;

        while (len0)
        {
            *dst++ = *src++;
            len0--;
        }
    }

    return save;
}



void *memmove(void *d, const void *s, size_t n)
{
    char *dest = d;
    const char *src  = s;

    if ((size_t) (dest - src) < n)
    {
        /*
         * The <src> buffer overlaps with the start of the <dest> buffer.
         * Copy backwards to prevent the premature corruption of <src>.
         */

        while (n > 0)
        {
            n--;
            dest[n] = src[n];
        }
    }
    else
    {
        /* It is safe to perform a forward-copy */
        while (n > 0)
        {
            *dest = *src;
            dest++;
            src++;
            n--;
        }
    }

    return d;
}



char *strcpy(char *d, const char *s)
{
	char *dest = d;
	while (*s != '\0')
    {
		*d = *s;
		d++;
		s++;
	}
	*d = '\0';
	return dest;
}



char *strcat(char *dest, const char *src)
{
	strcpy(dest + strlen(dest), src);
	return dest;
}



char *strchr(const char *s, int c)
{
    char tmp = (char) c;

    while ((*s != tmp) && (*s != '\0'))
        s++;

    return (*s == tmp) ? (char *) s : NULL;
}



size_t strcspn( const char * s1, const char * s2 )
{
    size_t len = 0;
    const char * p;
    while ( s1[len] )
    {
        p = s2;
        while ( *p )
        {
            if ( s1[len] == *p++ )
            {
                return len;
            }
        }
        ++len;
    }
    return len;
}



size_t strspn( const char * s1, const char * s2 )
{
    size_t len = 0;
    const char * p;
    while ( s1[ len ] )
    {
        p = s2;
        while ( *p )
        {
            if ( s1[len] == *p )
            {
                break;
            }
            ++p;
        }
        if ( ! *p )
        {
            return len;
        }
        ++len;
    }
    return len;
}
