#include <stdio.h>
#include <stdint.h>

int main()
{
    uint32_t last_value = *(volatile int *)0x00010000;

    while(1)
    {
        uint32_t value;
        while(1)
        {
            value = *(volatile int *)0x00010000;
            if (value != last_value)
            {
                break;
            }
        }

        printf("Read value %x\n", value);
        last_value = value;

        if (value == 0x12345678 + 9)
        {
            break;
        }
    }

    return 0;
}
