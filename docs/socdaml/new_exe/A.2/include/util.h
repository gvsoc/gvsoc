#ifndef UTIL_H
#define UTIL_H

#include "flex_printf.h"

void print_array_uint16(uint16_t * array, int rows, int cols)
{
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            printf("%d ", array[i*cols + j]);
        }
        printf("\n");
    }
}


#endif // UTIL_H