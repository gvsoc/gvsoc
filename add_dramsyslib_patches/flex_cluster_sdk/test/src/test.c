#include "snrt.h"
#include "flex_runtime.h"
#include "flex_redmule.h"
#include "flex_cluster_arch.h"
#include "flex_dma_pattern.h"

int main()
{
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    flex_eoc(1);
	return 0;
}