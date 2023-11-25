#include <stdio.h>
#include <stdint.h>

int main()
{
    printf("Hello, got 0x%x from my comp\n", *(uint32_t *)0x20000000);

    printf("Hello, got 0x%x from my comp\n", *(uint32_t *)0x20000004);

    *(uint32_t *)0x20000008 = 0x11223344;
    *(uint8_t *)0x20000009 = 0x77;

    printf("Hello, got 0x%x at 0x20000008\n", *(uint32_t *)0x20000008);

    *(volatile uint32_t *)0x20000100 = 0x12345678;
    *(volatile uint32_t *)0x20000104 = 0x12345678;

    return 0;
}
