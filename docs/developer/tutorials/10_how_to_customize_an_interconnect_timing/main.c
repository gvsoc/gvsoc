#include <stdio.h>
#include <stdint.h>

int main()
{
    for (int i=0; i<20; i++)
    {
        *(volatile uint32_t *)0x00000004;
        *(volatile uint32_t *)0x00000004;
        *(volatile uint32_t *)0x00000004;
        *(volatile uint32_t *)0x00000004;
    }

    return 0;
}
