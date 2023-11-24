#include "init.h"



typedef void (*fptr)(void);

static fptr ctor_list[1] __attribute__((section(".ctors.start"))) = { (fptr) -1 };
static fptr dtor_list[1] __attribute__((section(".dtors.start"))) = { (fptr) -1 };

int main();

static void __init_do_ctors(void)
{
    fptr *fpp;

    for(fpp = ctor_list+1;  *fpp != 0;  ++fpp)
    {
        (**fpp)();
    }
}

static void __init_do_dtors(void)
{
    fptr *fpp;
    for(fpp = dtor_list + 1;  *fpp != 0;  ++fpp)
    {
        (**fpp)();
    }
}

extern unsigned int _bss_start;
extern unsigned int _bss_end;

static inline void *__init_bss_start() { return (void *)&_bss_start; }
static inline void *__init_bss_end() { return (void *)&_bss_end; }

static __attribute__((noinline)) void __init_bss()
{
    unsigned int *bss = (unsigned int *)__init_bss_start();
    unsigned int *bss_end = (unsigned int *)__init_bss_end();

    while (bss != bss_end)
    {
        *bss++ = 0;
    }
}

void __init_stop(int status)
{
    __init_do_dtors();

    exit(status);
}
void __init_start()
{
    __init_bss();

    __mem_alloc_init_all();

    __init_do_ctors();

    int retval = main();

    __init_stop(retval);
}
