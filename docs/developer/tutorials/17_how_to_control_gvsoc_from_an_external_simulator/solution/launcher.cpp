#include <cstring>
#include <gv/gvsoc.hpp>


class MyLauncher : public gv::Io_user
{
public:
    int run(std::string config_path);

    // This gets called when an access from gvsoc side is reaching us
    void access(gv::Io_request *req);
    // This gets called when one of our access gets granted
    void grant(gv::Io_request *req);
    // This gets called when one of our access gets its response
    void reply(gv::Io_request *req);

private:
    gv::Io_binding *axi;
};



int main(int argc, char *argv[])
{
    char *config_path = NULL;

    for (int i=1; i<argc; i++)
    {
        if (strncmp(argv[i], "--config=", 9) == 0)
        {
            config_path = &argv[i][9];
        }
    }

    if (config_path == NULL)
    {
        fprintf(stderr, "No configuration specified, please specify through option --config=<config path>.\n");
        return -1;
    }

    MyLauncher launcher;

    return launcher.run(config_path);
}



int MyLauncher::run(std::string config_path)
{

    gv::GvsocConf conf = { .config_path=config_path, .api_mode=gv::Api_mode::Api_mode_sync };
    gv::Gvsoc *gvsoc = gv::gvsoc_new(&conf);
    gvsoc->open();

    // Get a connection to the main soc AXI. This will allow us to inject accesses
    // and could also be used to received accesses from simulated test
    // to a certain mapping corresponding to the external devices.
    this->axi = gvsoc->io_bind(this, "/soc/axi_proxy", "");
    if (this->axi == NULL)
    {
        fprintf(stderr, "Couldn't find AXI proxy\n");
        return -1;
    }

    gvsoc->start();

    // Run for 1ms to let the chip boot as it is not accessible before it is powered-up
    gvsoc->step(10000000000);

    // Writes few values to specific address, test will exit when it reads the last one
    for (int i=0; i<10; i++)
    {
        uint32_t value = 0x12345678 + i;

        gv::Io_request req;
        req.data = (uint8_t *)&value;
        req.size = 4;
        req.type = gv::Io_request_write;
        req.addr = 0x00010000;

        // The response is received through a function call to our access method.
        // This is done either directly from our function call to the axi access
        // method for simple accesses or asynchronously for complex accesses which
        // cannot be replied immediately (e.g. due to cache miss).
        this->axi->access(&req);

        // Wait for a while so that the simulated test sees the value and prints it
        gvsoc->step(1000000000);
    }

    // Wait for simulation termination and exit code returned by simulated test
    int retval = gvsoc->join();

    gvsoc->stop();
    gvsoc->close();

    return retval;
}



void MyLauncher::access(gv::Io_request *req)
{
    printf("Received request (is_read: %d, addr: 0x%lx, size: 0x%lx)\n", req->type == gv::Io_request_read, req->addr, req->size);

    if (req->type == gv::Io_request_read)
    {
        memset(req->data, req->addr, req->size);
    }

    this->axi->reply(req);
}



void MyLauncher::grant(gv::Io_request *req)
{
}



void MyLauncher::reply(gv::Io_request *req)
{
}