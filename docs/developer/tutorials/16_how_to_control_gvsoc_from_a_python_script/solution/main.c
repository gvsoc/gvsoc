#include <stdio.h>
#include <stdint.h>

int main()
{
    printf("Hello\n");

    *(volatile uint32_t *)0x00010000 = 0x12345678;

    while (*(volatile uint32_t *)0x00010000 == 0x12345678)
    {

    }

    printf("Leaving\n");

    return 0;
}
