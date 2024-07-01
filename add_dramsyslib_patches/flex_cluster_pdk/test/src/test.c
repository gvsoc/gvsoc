#include "snrt.h"
#include "eoc.h"

int main()
{
    snrt_cluster_hw_barrier();
    eoc(1);
	return 0;
}