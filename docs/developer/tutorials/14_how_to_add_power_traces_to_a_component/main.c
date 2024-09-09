#include <stdio.h>
#include <stdint.h>

int main()
{
    printf("Hello, got 0x%x from my comp\n", *(uint32_t *)0x20000000);

    int voltages[] = { 600, 800, 1200};

    for (int i=0; i<sizeof(voltages)/sizeof(int); i++)
    {
        printf("Voltage %d\n", voltages[i]);

        *(volatile uint32_t *)0x30000004 = voltages[i];

        // Mesure power when off and no clock
        printf("OFF\n");
        *(volatile uint32_t *)0x10000000 = 0xabbaabba;
        *(volatile uint32_t *)0x10000000 = 0xdeadcaca;


        // Mesure power when on and no clock
        printf("ON clock-gated\n");
        *(volatile uint32_t *)0x30000000 = 1;
        *(volatile uint32_t *)0x10000000 = 0xabbaabba;
        *(volatile uint32_t *)0x10000000 = 0xdeadcaca;


        // Mesure power when on and clock
        printf("ON\n");
        *(volatile uint32_t *)0x30000000 = 0x3;
        *(volatile uint32_t *)0x10000000 = 0xabbaabba;
        *(volatile uint32_t *)0x10000000 = 0xdeadcaca;


        // Mesure power with accesses
        printf("ON with accesses\n");
        *(volatile uint32_t *)0x10000000 = 0xabbaabba;
        for (int i=0; i<20; i++)
        {
            *(volatile uint32_t *)0x20000000 = i;
        }
        *(volatile uint32_t *)0x10000000 = 0xdeadcaca;


        *(volatile uint32_t *)0x30000000 = 0;

        printf("\n\n");
    }

    return 0;
}
