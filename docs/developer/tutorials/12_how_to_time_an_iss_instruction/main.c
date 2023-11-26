#include <stdio.h>
#include <stdint.h>


uint32_t my_instr(uint32_t a, uint32_t b);


int main()
{
    uint32_t a = 5;
    uint32_t b = 10;

    for (int i=0; i<5; i++)
    {
        uint32_t c = my_instr(a, b + i);
        printf("%d + 2 * %d -> %d\n", a, b + i, c);
    }

    return 0;
}
