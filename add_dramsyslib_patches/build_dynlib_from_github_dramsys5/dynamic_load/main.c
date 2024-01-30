#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>  // Linux specific header for dynamic loading

uint64_t buf[64] = {1,2,3,4,5};
uint64_t rec[64] = {0,0,0,0,0};

#define TXN_LEN 64

void print_data(uint64_t * buf){
    int i;
    for (i = 0; i < 10; ++i)
    {
        printf("%d, ", buf[i]);
    }
    printf("\n");
}

int main() {
    void* libraryHandle;
    int (*add_dram)(char *, char *);
    void (*cloes_dram)(int);
    int (*dram_can_accept_req)(int);
    void (*dram_write_buffer)(int dram_id, int byte_int, int idx);
    void (*dram_write_strobe)(int dram_id, int strob_int, int idx);
    int (*dram_has_read_rsp)(int dram_id);
    void (*dram_send_req)(int dram_id, uint64_t addr, uint64_t length , uint64_t is_write, uint64_t strob_enable);
    void (*dram_get_read_rsp)(int dram_id, uint64_t length, const void * buf);
    void (*run_ns)(int ns);

    printf("load library --- \n");
    libraryHandle = dlopen("third_party/DRAMSys/libDRAMSys_Simulator.so", RTLD_LAZY);

    printf("get function --- \n");
    add_dram = dlsym(libraryHandle, "add_dram");
    cloes_dram = dlsym(libraryHandle, "cloes_dram");
    dram_can_accept_req = dlsym(libraryHandle, "dram_can_accept_req");
    dram_has_read_rsp = dlsym(libraryHandle, "dram_has_read_rsp");
    dram_send_req = dlsym(libraryHandle, "dram_send_req");
    dram_get_read_rsp = dlsym(libraryHandle, "dram_get_read_rsp");
    dram_write_buffer = dlsym(libraryHandle, "dram_write_buffer");
    dram_write_strobe = dlsym(libraryHandle, "dram_write_strobe");
    run_ns = dlsym(libraryHandle, "run_ns");

    

    printf("use function --- \n");

    // Use the function from the dynamic library
    int dram_id = add_dram("add_dramsyslib_patches/dramsys_configs", "add_dramsyslib_patches/dramsys_configs/hbm2-example.json");
    int dram_id2 = add_dram("add_dramsyslib_patches/dramsys_configs", "add_dramsyslib_patches/dramsys_configs/hbm2-example.json");
    run_ns(1000);
    printf("get dram id: %d\n", dram_id);

    print_data(buf);
    print_data(rec);

    //try to write something
    if (dram_can_accept_req(dram_id))
    {
        int i;
        printf("send write req \n");
        uint8_t * byte_buf = (uint8_t *)buf;
        for (i = 0; i < TXN_LEN; ++i)
        {
            dram_write_buffer(dram_id,byte_buf[i],i);
            if (i<10)
            {
                dram_write_strobe(dram_id, 0, i);
            }else{
                dram_write_strobe(dram_id, 1, i);
            }
        }
        dram_send_req(dram_id, 0, TXN_LEN, 1, 1);
    }

    //run 1000ns
    run_ns(1000);

    if (dram_can_accept_req(dram_id))
    {
        printf("send read req \n");
        dram_send_req(dram_id, 0, TXN_LEN, 0, 0);
    }

    //run 1000ns
    run_ns(1000);

    printf("check read resp  \n");

    if (dram_has_read_rsp(dram_id))
    {
        printf("get read response \n");
        dram_get_read_rsp(dram_id, TXN_LEN, (void *)rec);
    }

    print_data(rec);

    cloes_dram(dram_id);
    printf("close dram: %d\n", dram_id);

    // Unload the dynamic library
    dlclose(libraryHandle);

    return 0;
}
