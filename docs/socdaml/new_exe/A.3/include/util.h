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

void check_results_uint16(uint16_t * array, uint16_t * golden, int rows, int cols)
{
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            if (array[i*cols + j] != golden[i*cols + j])
            {
                printf("Results Incorrect: Detect the first error at row %d, colunm %d, expect %d but get %d\n", i, j, golden[i*cols + j], array[i*cols + j]);
                return;
            }
        }
    }
    printf("Results Correct!\n");
}


#endif // UTIL_H