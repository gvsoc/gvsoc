#include <stdio.h>
#include <stdint.h>

int main()
{
    printf("Hello, got 0x%x from my comp\n", *(uint32_t *)0x20000000);

    for (int i=0; i<20; i++)
    {
        *(volatile uint32_t *)0x20000000 = i;
    }

    return 0;
}
