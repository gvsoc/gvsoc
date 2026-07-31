// Copyright 2025 ETH Zurich and 
// University of Bologna

// Solderpad Hardware License
// Version 0.51, see LICENSE for details.

// SPDX-License-Identifier: SHL-0.51

// Author: Chi Zhang <chizhang@iis.ee.ethz.ch>, ETH Zurich
// Date: 1.Oct.2025

#include "flex_runtime.h"
#include "CollectiveCompare.h"

int main()
{
    uint32_t eoc_val = 0;
    flex_barrier_xy_init();
    flex_global_barrier_xy();
    /**************************************/
    /*  Program Execution Region -- Start */
    /**************************************/

    CollectiveCompare info = analyze();
    flex_global_barrier_xy();
    if (info.cluster_global_id == 0 && flex_is_first_core()) printf("[Rowwise Multicast Testing]\n");
    flex_global_barrier_xy();

    uint32_t size = 256;
    for (int i = 0; i < 11; ++i)
    {
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("--[Size = %d Bytes]\n", size);



        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe1]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_broadcast_rowwise(&info, 0, size, 1);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe2]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_broadcast_rowwise(&info, 0, size, 2);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe4]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_broadcast_rowwise(&info, 0, size, 4);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe8]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_broadcast_rowwise(&info, 0, size, 8);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe16]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_broadcast_rowwise(&info, 0, size, 16);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe32]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_broadcast_rowwise(&info, 0, size, 32);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Tree]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_logtree_broadcast_rowwise(&info, 0, size);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Sequence]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_multiunicast_broadcast_rowwise(&info, 0, size);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();


        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[HWColl]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) flat_attention_broadcast_rowwise(&info, 0, size);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();



        flex_global_barrier_xy();
        size = size * 2;
    }







    flex_global_barrier_xy();
    if (info.cluster_global_id == 0 && flex_is_first_core()) printf("[Rowwise Reduction Testing]\n");
    flex_global_barrier_xy();

    size = 8;
    for (int i = 0; i < 16; ++i)
    {
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("--[Size = %d Bytes]\n", size);



        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe1]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_redsum_rowwise(&info, 0, size, 1);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe2]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_redsum_rowwise(&info, 0, size, 2);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe4]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_redsum_rowwise(&info, 0, size, 4);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe8]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_redsum_rowwise(&info, 0, size, 8);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe16]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_redsum_rowwise(&info, 0, size, 16);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Pipe32]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_pipeline_redsum_rowwise(&info, 0, size, 32);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Tree]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_logtree_redsum_rowwise(&info, 0, size);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();

        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[Sequence]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) swcoll_multiunicast_redsum_rowwise(&info, 0, size);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();


        flex_global_barrier_xy();
        if (info.cluster_global_id == 0 && flex_is_first_core()) printf("----[HWColl]\n");
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
        flex_global_barrier_xy();
        for (int a = 0; a < 10; ++a) flat_attention_redsum_rowwise(&info, 0, size);
        flex_global_barrier_xy();
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();



        flex_global_barrier_xy();
        size = size * 2;
    }


    /**************************************/
    /*  Program Execution Region -- Stop  */
    /**************************************/
    flex_global_barrier_xy();
    flex_eoc(eoc_val);
    return 0;
}