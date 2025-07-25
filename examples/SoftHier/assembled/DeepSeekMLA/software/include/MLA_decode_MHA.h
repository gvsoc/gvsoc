#ifndef _MLA_DECODE_MHA_H_
#define _MLA_DECODE_MHA_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"

#define TILE_DIM 128
#define K_DIM    128
#define HEAD_NUM 128
#define COMP_DIM 512
#define ROPE_DIM 64

#define DATA_TYPE_BYTE       2
#define DATA_TYPE_WIDTH      16
#define REDMULE_COMPUTE_TYPE REDMULE_NONE_16

// #define DATA_TYPE_BYTE       1
// #define DATA_TYPE_WIDTH      8
// #define REDMULE_COMPUTE_TYPE REDMULE_FP_8

#define STR(x) #x
#define XSTR(x) STR(x)

#include "MLA_util.h"

typedef struct MLADecodeMHAInfo
{
    //General information
    uint64_t                MetaQ_base_address;
    uint64_t                CKVR_base_address;
    uint64_t                Output_base_address;
    uint32_t                speculative_length;
    uint32_t                kv_sequence_length;
    uint32_t                meta_head_dim;
    uint32_t                O_head_dim;
    uint32_t                batch_size;
    uint32_t                elem_size;

    //Group infomation
    uint32_t                flatten_scale_x;
    uint32_t                flatten_scale_y;
    GridSyncGroupInfo       group;

    //Cluster information
    FlexPosition            cluster_pos;
    uint32_t                cluster_global_id;
    uint32_t                cluster_in_group_id;
    uint32_t                cluster_in_group_id_x;
    uint32_t                cluster_in_group_id_y;

    //Tiling information
    uint32_t                Bc_s;
    uint32_t                Br_s;
    uint32_t                Tk_sv;
    uint32_t                Tk_qk;
    uint32_t                Tc;
    uint32_t                Tr;
    uint32_t                d;
    uint32_t                d_rope;

    //Addressing Information
    uint64_t                batch_per_group;
    uint64_t                Q_matrix_size_per_batch;
    uint64_t                C_matrix_size_per_batch;
    uint64_t                O_matrix_size_per_batch;
    uint64_t                Q_sub_matrix_size_per_batch;
    uint64_t                C_sub_matrix_size_per_batch;
    uint64_t                O_sub_matrix_size_per_batch;
    uint64_t                Q_matrix_start_base_batch_1;
    uint64_t                C_matrix_start_base_batch_1;
    uint64_t                O_matrix_start_base_batch_1;
    uint64_t                Q_matrix_start_base_batch_2;
    uint64_t                C_matrix_start_base_batch_2;
    uint64_t                O_matrix_start_base_batch_2;
    uint64_t                Q_tile_start_base_batch_1;
    uint64_t                C_tile_start_base_batch_1;
    uint64_t                O_tile_start_base_batch_1;
    uint64_t                Q_tile_start_base_batch_2;
    uint64_t                C_tile_start_base_batch_2;
    uint64_t                O_tile_start_base_batch_2;
    uint64_t                Q_tile_tmp_base_batch_1;
    uint64_t                C_tile_tmp_base_batch_1;
    uint64_t                O_tile_tmp_base_batch_1;
    uint64_t                Q_tile_tmp_base_batch_2;
    uint64_t                C_tile_tmp_base_batch_2;
    uint64_t                O_tile_tmp_base_batch_2;

    //L1 location information
    uint32_t                L1_Q1;
    uint32_t                L1_K1;
    uint32_t                L1_V1;
    uint32_t                L1_O1;
    uint32_t                L1_Q2;
    uint32_t                L1_K2;
    uint32_t                L1_V2;
    uint32_t                L1_O2;
    uint32_t                L1_S1;
    uint32_t                L1_S2;
    uint32_t                L1_m;
    uint32_t                L1_l;

    //Usefull parameters for address calculation
    uint32_t                L1_Q_size;
    uint32_t                L1_K_size;
    uint32_t                L1_V_size;
    uint32_t                L1_O_size;
    uint32_t                L1_A_size;
    uint32_t                L1_m_size;
    uint32_t                L1_l_size;

    //validation
    uint32_t                valid;

}MLADecodeMHAInfo;

MLADecodeMHAInfo MLA_Decode_MHA_analyze(
    uint64_t                MetaQ_base_address,
    uint64_t                CKVR_base_address,
    uint64_t                Output_base_address,
    uint32_t                speculative_length,
    uint32_t                kv_sequence_length,
    uint32_t                batch_size,
    uint32_t                elem_size){

    MLADecodeMHAInfo info;

    //General information
    info.MetaQ_base_address         = MetaQ_base_address;
    info.CKVR_base_address          = CKVR_base_address;
    info.Output_base_address        = Output_base_address;
    info.speculative_length         = speculative_length;
    info.kv_sequence_length         = kv_sequence_length;
    info.meta_head_dim              = COMP_DIM + ROPE_DIM;
    info.O_head_dim                 = COMP_DIM;
    info.batch_size                 = batch_size;
    info.elem_size                  = elem_size;

    //Group infomation
    info.flatten_scale_x            = (kv_sequence_length + TILE_DIM - 1) / TILE_DIM;
    info.flatten_scale_x            = (info.flatten_scale_x > ARCH_NUM_CLUSTER_X)? ARCH_NUM_CLUSTER_X : info.flatten_scale_x;
    info.flatten_scale_y            = (speculative_length * HEAD_NUM + TILE_DIM - 1) / TILE_DIM;
    info.flatten_scale_y            = (info.flatten_scale_y > ARCH_NUM_CLUSTER_Y)? ARCH_NUM_CLUSTER_Y : info.flatten_scale_y;
    info.group                      = grid_sync_group_init(info.flatten_scale_x,info.flatten_scale_y);

    //Cluster information
    FlexPosition pos                = get_pos(flex_get_cluster_id());
    info.cluster_global_id          = flex_get_cluster_id();
    info.cluster_pos                = pos;
    info.cluster_in_group_id_x      = pos.x % info.group.grid_x_dim;
    info.cluster_in_group_id_y      = pos.y % info.group.grid_y_dim;
    info.cluster_in_group_id        = info.cluster_in_group_id_x + info.group.this_grid_cluster_num_x * info.cluster_in_group_id_y;

    //Tiling information
    info.Bc_s                       = TILE_DIM;
    info.Br_s                       = TILE_DIM;
    info.Tk_sv                      = (COMP_DIM + K_DIM - 1) / K_DIM;
    info.Tk_qk                      = info.Tk_sv + 1;
    info.Tc                         = (kv_sequence_length + (info.flatten_scale_x * TILE_DIM) - 1) / (info.flatten_scale_x * TILE_DIM);
    info.Tr                         = (speculative_length * HEAD_NUM + (info.flatten_scale_y * TILE_DIM) - 1) / (info.flatten_scale_y * TILE_DIM);
    info.d                          = K_DIM;
    info.d_rope                     = ROPE_DIM;

    //Addressing Information
    info.batch_per_group            = (info.batch_size + (info.group.grid_x_num * info.group.grid_y_num) - 1) / (info.group.grid_x_num * info.group.grid_y_num);
    info.Q_matrix_size_per_batch    = speculative_length    * HEAD_NUM  * (COMP_DIM + ROPE_DIM) * info.elem_size;
    info.C_matrix_size_per_batch    = kv_sequence_length                * (COMP_DIM + ROPE_DIM) * info.elem_size;
    info.O_matrix_size_per_batch    = speculative_length    * HEAD_NUM  *  COMP_DIM             * info.elem_size;

    info.Q_sub_matrix_size_per_batch= info.flatten_scale_y  * TILE_DIM  * (COMP_DIM + ROPE_DIM) * info.elem_size;
    info.C_sub_matrix_size_per_batch= info.flatten_scale_x  * TILE_DIM  * (COMP_DIM + ROPE_DIM) * info.elem_size;
    info.O_sub_matrix_size_per_batch= info.flatten_scale_y  * TILE_DIM  *  COMP_DIM             * info.elem_size;

    info.Q_matrix_start_base_batch_1= info.MetaQ_base_address           + info.batch_per_group          * info.group.this_grid_id           * info.Q_matrix_size_per_batch;
    info.C_matrix_start_base_batch_1= info.CKVR_base_address            + info.batch_per_group          * info.group.this_grid_id           * info.C_matrix_size_per_batch;
    info.O_matrix_start_base_batch_1= info.Output_base_address          + info.batch_per_group          * info.group.this_grid_id           * info.O_matrix_size_per_batch;
    info.Q_matrix_start_base_batch_2= info.MetaQ_base_address           + (info.batch_per_group         * info.group.this_grid_id + 1)      * info.Q_matrix_size_per_batch;
    info.C_matrix_start_base_batch_2= info.CKVR_base_address            + (info.batch_per_group         * info.group.this_grid_id + 1)      * info.C_matrix_size_per_batch;
    info.O_matrix_start_base_batch_2= info.Output_base_address          + (info.batch_per_group         * info.group.this_grid_id + 1)      * info.O_matrix_size_per_batch;

    info.Q_tile_start_base_batch_1  = info.Q_matrix_start_base_batch_1  + info.cluster_in_group_id_y    * (COMP_DIM + ROPE_DIM) * TILE_DIM  * info.elem_size;
    info.C_tile_start_base_batch_1  = info.C_matrix_start_base_batch_1  + info.cluster_in_group_id_x    * (COMP_DIM + ROPE_DIM) * TILE_DIM  * info.elem_size;
    info.O_tile_start_base_batch_1  = info.O_matrix_start_base_batch_1  + info.cluster_in_group_id_y    *  COMP_DIM             * TILE_DIM  * info.elem_size;
    info.Q_tile_start_base_batch_2  = info.Q_matrix_start_base_batch_2  + info.cluster_in_group_id_y    * (COMP_DIM + ROPE_DIM) * TILE_DIM  * info.elem_size;
    info.C_tile_start_base_batch_2  = info.C_matrix_start_base_batch_2  + info.cluster_in_group_id_x    * (COMP_DIM + ROPE_DIM) * TILE_DIM  * info.elem_size;
    info.O_tile_start_base_batch_2  = info.O_matrix_start_base_batch_2  + info.cluster_in_group_id_y    *  COMP_DIM             * TILE_DIM  * info.elem_size;

    info.Q_tile_tmp_base_batch_1    = info.Q_tile_start_base_batch_1;
    info.C_tile_tmp_base_batch_1    = info.C_tile_start_base_batch_1;
    info.O_tile_tmp_base_batch_1    = info.O_tile_start_base_batch_1;
    info.Q_tile_tmp_base_batch_2    = info.Q_tile_start_base_batch_2;
    info.C_tile_tmp_base_batch_2    = info.C_tile_start_base_batch_2;
    info.O_tile_tmp_base_batch_2    = info.O_tile_start_base_batch_2;

    //L1 location information
    info.L1_Q1                      = local(0);
    info.L1_K1                      = info.L1_Q1    + info.Br_s     * info.d    * info.elem_size;
    info.L1_V1                      = info.L1_K1    + info.Bc_s     * info.d    * info.elem_size;
    info.L1_O1                      = info.L1_V1    + info.Bc_s     * info.d    * info.elem_size;
    info.L1_Q2                      = info.L1_O1    + info.Br_s     * info.d    * info.elem_size;
    info.L1_K2                      = info.L1_Q2    + info.Br_s     * info.d    * info.elem_size;
    info.L1_V2                      = info.L1_K2    + info.Bc_s     * info.d    * info.elem_size;
    info.L1_O2                      = info.L1_V2    + info.Bc_s     * info.d    * info.elem_size;
    info.L1_S1                      = info.L1_O2    + info.Br_s     * info.d    * info.elem_size;
    info.L1_S2                      = info.L1_S1    + info.Br_s     * info.Bc_s * info.elem_size;
    info.L1_m                       = info.L1_S2    + info.Br_s     * info.Bc_s * info.elem_size;
    info.L1_l                       = info.L1_m     + info.Br_s     * 1         * info.elem_size;

    //Usefull parameters for data layout address calculation
    info.L1_Q_size                  = info.Br_s * info.d    * info.elem_size;
    info.L1_K_size                  = info.Bc_s * info.d    * info.elem_size;
    info.L1_V_size                  = info.Bc_s * info.d    * info.elem_size;
    info.L1_O_size                  = info.Br_s * info.d    * info.elem_size;
    info.L1_A_size                  = info.Br_s * info.Bc_s * info.elem_size;
    info.L1_m_size                  = info.Br_s * 1         * info.elem_size;
    info.L1_l_size                  = info.Br_s * 1         * info.elem_size;

    //validation check
    info.valid = 1;
    if (info.group.valid_grid == 0) info.valid = 0;
    if (info.flatten_scale_x > ARCH_NUM_CLUSTER_X) info.valid = 0;
    if (info.flatten_scale_y > ARCH_NUM_CLUSTER_Y) info.valid = 0;
    if (info.batch_per_group < 2) info.valid = 0;

    return info;
}

void MLA_print_info(MLADecodeMHAInfo *info) {
    uint32_t H, L;

    // General Information
    printf("    General Information\n");
    H = info->MetaQ_base_address >> 32;
    L = (uint32_t)(info->MetaQ_base_address);
    printf("        MetaQ_base_address:             0x%08x_%08x\n", H, L);
    H = info->CKVR_base_address >> 32;
    L = (uint32_t)(info->CKVR_base_address);
    printf("        CKVR_base_address:              0x%08x_%08x\n", H, L);
    H = info->Output_base_address >> 32;
    L = (uint32_t)(info->Output_base_address);
    printf("        Output_base_address:            0x%08x_%08x\n", H, L);
    printf("        speculative_length:             %u\n", info->speculative_length);
    printf("        kv_sequence_length:             %u\n", info->kv_sequence_length);
    printf("        meta_head_dim:                  %u\n", info->meta_head_dim);
    printf("        batch_size:                     %u\n", info->batch_size);
    printf("        elem_size:                      %u\n", info->elem_size);

    // Group Information
    printf("    Group Information\n");
    printf("        flatten_scale_x:                %u\n", info->flatten_scale_x);
    printf("        flatten_scale_y:                %u\n", info->flatten_scale_y);
    printf("        group.this_grid_id:             %u\n", info->group.this_grid_id);
    // GridSyncGroupInfo skipped

    // Cluster Information
    printf("    Cluster Information\n");
    // FlexPosition skipped
    printf("        cluster_global_id:              %u\n", info->cluster_global_id);
    printf("        cluster_in_group_id:            %u\n", info->cluster_in_group_id);
    printf("        cluster_in_group_id_x:          %u\n", info->cluster_in_group_id_x);
    printf("        cluster_in_group_id_y:          %u\n", info->cluster_in_group_id_y);

    // Tiling Information
    printf("    Tiling Information\n");
    printf("        Bc_s:                           %u\n", info->Bc_s);
    printf("        Br_s:                           %u\n", info->Br_s);
    printf("        Tk_sv:                          %u\n", info->Tk_sv);
    printf("        Tk_qk:                          %u\n", info->Tk_qk);
    printf("        Tc:                             %u\n", info->Tc);
    printf("        Tr:                             %u\n", info->Tr);
    printf("        d:                              %u\n", info->d);
    printf("        d_rope:                         %u\n", info->d_rope);

    // Addressing Information
    printf("    Addressing Information\n");
#define PRINT_U64_FIELD(name) \
    H = info->name >> 32; \
    L = (uint32_t)(info->name); \
    printf("        %-32s 0x%08x_%08x\n", #name ":", H, L);

    PRINT_U64_FIELD(batch_per_group)
    PRINT_U64_FIELD(Q_matrix_size_per_batch)
    PRINT_U64_FIELD(C_matrix_size_per_batch)
    PRINT_U64_FIELD(O_matrix_size_per_batch)
    PRINT_U64_FIELD(Q_sub_matrix_size_per_batch)
    PRINT_U64_FIELD(C_sub_matrix_size_per_batch)
    PRINT_U64_FIELD(O_sub_matrix_size_per_batch)
    PRINT_U64_FIELD(Q_matrix_start_base_batch_1)
    PRINT_U64_FIELD(C_matrix_start_base_batch_1)
    PRINT_U64_FIELD(O_matrix_start_base_batch_1)
    PRINT_U64_FIELD(Q_matrix_start_base_batch_2)
    PRINT_U64_FIELD(C_matrix_start_base_batch_2)
    PRINT_U64_FIELD(O_matrix_start_base_batch_2)
    PRINT_U64_FIELD(Q_tile_start_base_batch_1)
    PRINT_U64_FIELD(C_tile_start_base_batch_1)
    PRINT_U64_FIELD(O_tile_start_base_batch_1)
    PRINT_U64_FIELD(Q_tile_start_base_batch_2)
    PRINT_U64_FIELD(C_tile_start_base_batch_2)
    PRINT_U64_FIELD(O_tile_start_base_batch_2)

    // L1 Location Information
    printf("    L1 Location Information\n");
    printf("        L1_Q1:                          0x%08x\n", info->L1_Q1);
    printf("        L1_K1:                          0x%08x\n", info->L1_K1);
    printf("        L1_V1:                          0x%08x\n", info->L1_V1);
    printf("        L1_O1:                          0x%08x\n", info->L1_O1);
    printf("        L1_Q2:                          0x%08x\n", info->L1_Q2);
    printf("        L1_K2:                          0x%08x\n", info->L1_K2);
    printf("        L1_V2:                          0x%08x\n", info->L1_V2);
    printf("        L1_O2:                          0x%08x\n", info->L1_O2);
    printf("        L1_S1:                          0x%08x\n", info->L1_S1);
    printf("        L1_S2:                          0x%08x\n", info->L1_S2);
    printf("        L1_m:                           0x%08x\n", info->L1_m);
    printf("        L1_l:                           0x%08x\n", info->L1_l);

    // Useful Parameters
    printf("    Useful Parameters\n");
    printf("        L1_Q_size:                      0x%08x\n", info->L1_Q_size);
    printf("        L1_K_size:                      0x%08x\n", info->L1_K_size);
    printf("        L1_V_size:                      0x%08x\n", info->L1_V_size);
    printf("        L1_O_size:                      0x%08x\n", info->L1_O_size);
    printf("        L1_A_size:                      0x%08x\n", info->L1_A_size);
    printf("        L1_m_size:                      0x%08x\n", info->L1_m_size);
    printf("        L1_l_size:                      0x%08x\n", info->L1_l_size);

    // Validation
    printf("    Validation\n");
    printf("        valid:                          %u\n", info->valid);

#undef PRINT_U64_FIELD
}

int MLA_Decode_MHA_run(MLADecodeMHAInfo * info);

int MLA_Decode_MHA(
    uint64_t                MetaQ_base_address,
    uint64_t                CKVR_base_address,
    uint64_t                Output_base_address,
    uint32_t                speculative_length,
    uint32_t                kv_sequence_length,
    uint32_t                batch_size,
    uint32_t                elem_size){

    flex_global_barrier_xy();

    MLADecodeMHAInfo info =  MLA_Decode_MHA_analyze(
                                MetaQ_base_address,
                                CKVR_base_address,
                                Output_base_address,
                                speculative_length,
                                kv_sequence_length,
                                batch_size,
                                elem_size);

    if (info.valid == 0)
    {
        if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0)
        {
            printf("Invalid Configuration\n");
        }
        return -1;
    }

    // for (int cid = 0; cid < ARCH_NUM_CLUSTER; ++cid)
    // {
    //     if (flex_get_core_id() == 0 && flex_get_cluster_id() == cid)
    //     {
    //         printf("[Cluster %3d] ---------------------------------------\n", cid);
    //         MLA_print_info(&info);
    //     }
    //     flex_global_barrier_xy();//Global barrier
    // }
    // flex_global_barrier_xy();

    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_start();
    flex_global_barrier_xy();
    MLA_Decode_MHA_run(&info);
    flex_global_barrier_xy();
    if (flex_get_core_id() == 0 && flex_get_cluster_id() == 0) flex_timer_end();
    flex_global_barrier_xy();
    return 0;
}

int MLA_Decode_MHA_run(MLADecodeMHAInfo * info)
{
    uint32_t recorded_store = 0;
    uint64_t recorded_store_addr;
    for (int b = 0; b < (info->batch_per_group / 2); ++b)
    {
        for (int r = 0; r < info->Tr; ++r)
        {
            for (int c = 0; c < info->Tc; ++c)
            {
                if (b != 0)
                {
                    info->Q_tile_start_base_batch_1 += 2 * info->Q_matrix_size_per_batch;
                    info->C_tile_start_base_batch_1 += 2 * info->C_matrix_size_per_batch;
                    info->O_tile_start_base_batch_1 += 2 * info->O_matrix_size_per_batch;
                    info->Q_tile_start_base_batch_2 += 2 * info->Q_matrix_size_per_batch;
                    info->C_tile_start_base_batch_2 += 2 * info->C_matrix_size_per_batch;
                    info->O_tile_start_base_batch_2 += 2 * info->O_matrix_size_per_batch;
                }
                info->Q_tile_tmp_base_batch_1    = info->Q_tile_start_base_batch_1 + r * info->Q_sub_matrix_size_per_batch;
                info->O_tile_tmp_base_batch_1    = info->O_tile_start_base_batch_1 + r * info->O_sub_matrix_size_per_batch;
                info->Q_tile_tmp_base_batch_2    = info->Q_tile_start_base_batch_2 + r * info->Q_sub_matrix_size_per_batch;
                info->O_tile_tmp_base_batch_2    = info->O_tile_start_base_batch_2 + r * info->O_sub_matrix_size_per_batch;
                info->C_tile_tmp_base_batch_1    = info->C_tile_start_base_batch_1 + c * info->C_sub_matrix_size_per_batch;
                info->C_tile_tmp_base_batch_2    = info->C_tile_start_base_batch_2 + c * info->C_sub_matrix_size_per_batch;

                /************************/
                /*                      */
                /*      Phase 1-1       */
                /*      Preloading      */
                /*                      */
                /************************/

                //Preloading
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->Q_tile_tmp_base_batch_1, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->C_tile_tmp_base_batch_1, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-1       */
                /*      B1_QK_1         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->Q_tile_tmp_base_batch_1 + 1 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->C_tile_tmp_base_batch_1 + 1 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }

                    if (recorded_store != 0)
                    {
                        if (flex_is_dm_core())
                        {
                            if (info->cluster_in_group_id_x == 0)
                            {
                                //Reduction O
                                flex_dma_async_reduction(
                                    info->L1_O2/*dst_offset*/,
                                    info->L1_O2/*src_offset*/,
                                    info->L1_O_size/*transfer_size*/,
                                    COLLECTIVE_REDADD_FP_16/*fmt*/,
                                    info->group.wakeup_row_mask/*row_mask*/,
                                    (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                                //Store O
                                flex_dma_async_2d(
                                    recorded_store_addr, /*destination*/
                                    info->L1_O2, /*source*/
                                    info->d * info->elem_size, /*transfer size*/
                                    info->O_head_dim, /*destination stride*/
                                    info->d * info->elem_size, /*source stride*/
                                    info->Br_s /*repeat*/); //Start 2D iDMA
                                flex_dma_async_wait_all(); // Wait for iDMA Finishing
                            }
                        }
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-2       */
                /*      B1_QK_2         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->Q_tile_tmp_base_batch_1 + 2 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->C_tile_tmp_base_batch_1 + 2 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-3       */
                /*      B1_QK_3         */
                /*                      */
                /************************/


                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->Q_tile_tmp_base_batch_1 + 3 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->C_tile_tmp_base_batch_1 + 3 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-4       */
                /*      B1_QK_4         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->Q_tile_tmp_base_batch_1 + 4 * info->d * info->elem_size, /*source*/
                            info->d_rope * info->elem_size, /*transfer size*/
                            info->d_rope * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->Br_s * info->d_rope * info->elem_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->C_tile_tmp_base_batch_1 + 4 * info->d * info->elem_size, /*source*/
                            info->d_rope * info->elem_size, /*transfer size*/
                            info->d_rope * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->Bc_s * info->d_rope * info->elem_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 2-5       */
                /*      B1_QK_5         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d_rope, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->Q_tile_tmp_base_batch_2 + 0 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 0 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 3-1       */
                /*      B2_QK_1         */
                /*                      */
                /************************/


                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S2, REDMULE_COMPUTE_TYPE);
                    //Local Maxima
                    MLA_vector_rowmax_MV_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S1,
                        info->L1_m,
                        info->L1_m
                    );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->Q_tile_tmp_base_batch_2 + 1 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 1 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_dm_core())
                {
                    //Global maxima
                    flex_dma_async_reduction(
                        info->L1_m/*dst_offset*/,
                        info->L1_m/*src_offset*/,
                        info->L1_m_size/*transfer_size*/,
                        COLLECTIVE_REDMAX_FP_16/*fmt*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    flex_dma_async_broadcast(
                        info->L1_m/*dst_offset*/,
                        info->L1_m/*src_offset*/,
                        info->L1_m_size/*transfer_size*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }


                /************************/
                /*                      */
                /*      Phase 3-2       */
                /*      B2_QK_2         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S2, REDMULE_COMPUTE_TYPE);
                    //p = exp(att - m)
                    MLA_vector_EXP_MV(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S1,
                        info->L1_m
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->Q_tile_tmp_base_batch_2 + 2 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 2 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 3-3       */
                /*      B2_QK_3         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S2, REDMULE_COMPUTE_TYPE);
                    //l = rowsum(p)
                    MLA_vector_rowsum_M_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S1,
                        info->L1_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q1, /*destination*/
                            info->Q_tile_tmp_base_batch_2 + 3 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q1/*dst_offset*/,
                            info->L1_Q1/*src_offset*/,
                            info->L1_Q_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K1, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 3 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K1/*dst_offset*/,
                            info->L1_K1/*src_offset*/,
                            info->L1_K_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_dm_core())
                {
                    //Global Denominator
                    flex_dma_async_reduction(
                        info->L1_l/*dst_offset*/,
                        info->L1_l/*src_offset*/,
                        info->L1_l_size/*transfer_size*/,
                        COLLECTIVE_REDADD_FP_16/*fmt*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    flex_dma_async_broadcast(
                        info->L1_l/*dst_offset*/,
                        info->L1_l/*src_offset*/,
                        info->L1_l_size/*transfer_size*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }


                /************************/
                /*                      */
                /*      Phase 3-4       */
                /*      B2_QK_4         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_Q1, info->L1_K1, info->L1_S2, REDMULE_COMPUTE_TYPE);
                    //S = S/l
                    MLA_vector_M_div_V(
                        info->Br_s,
                        info->Bc_s,
                        info->L1_S1,
                        info->L1_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //load Q from west edge
                        flex_dma_async_2d(
                            info->L1_Q2, /*destination*/
                            info->Q_tile_tmp_base_batch_2 + 4 * info->d * info->elem_size, /*source*/
                            info->d_rope * info->elem_size, /*transfer size*/
                            info->d_rope * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //row-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_Q2/*dst_offset*/,
                            info->L1_Q2/*src_offset*/,
                            info->Br_s * info->d_rope * info->elem_size/*transfer_size*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_K2, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 4 * info->d * info->elem_size, /*source*/
                            info->d_rope * info->elem_size, /*transfer size*/
                            info->d_rope * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_K2/*dst_offset*/,
                            info->L1_K2/*src_offset*/,
                            info->Bc_s * info->d_rope * info->elem_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 3-5       */
                /*      B2_QK_5         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->d_rope, info->Bc_s);
                    flex_redmule_trigger(info->L1_Q2, info->L1_K2, info->L1_S2, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->C_tile_tmp_base_batch_1 + 0 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 4-1       */
                /*      B1_SV_1         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_config(info->Br_s, info->Bc_s, info->d);
                    flex_redmule_trigger(info->L1_S1, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                    //Local Maxima
                    MLA_vector_rowmax_MV_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S2,
                        info->L1_m,
                        info->L1_m
                    );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->C_tile_tmp_base_batch_1 + 1 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_dm_core())
                {
                    //Global maxima
                    flex_dma_async_reduction(
                        info->L1_m/*dst_offset*/,
                        info->L1_m/*src_offset*/,
                        info->L1_m_size/*transfer_size*/,
                        COLLECTIVE_REDMAX_FP_16/*fmt*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    flex_dma_async_broadcast(
                        info->L1_m/*dst_offset*/,
                        info->L1_m/*src_offset*/,
                        info->L1_m_size/*transfer_size*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }


                /************************/
                /*                      */
                /*      Phase 4-2       */
                /*      B1_SV_2         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                    //p = exp(att - m)
                    MLA_vector_EXP_MV(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S2,
                        info->L1_m
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->C_tile_tmp_base_batch_1 + 2 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O1/*dst_offset*/,
                            info->L1_O1/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDADD_FP_16/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->O_tile_tmp_base_batch_1 + 0 * info->d * info->elem_size, /*destination*/
                            info->L1_O1, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->O_head_dim, /*destination stride*/
                            info->d * info->elem_size, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 4-3       */
                /*      B1_SV_3         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                    //l = rowsum(p)
                    MLA_vector_rowsum_M_V(
                        info->Bc_s,
                        info->Br_s,
                        info->L1_S2,
                        info->L1_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->C_tile_tmp_base_batch_1 + 3 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O2/*dst_offset*/,
                            info->L1_O2/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDADD_FP_16/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->O_tile_tmp_base_batch_1 + 1 * info->d * info->elem_size, /*destination*/
                            info->L1_O2, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->O_head_dim, /*destination stride*/
                            info->d * info->elem_size, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }
                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_dm_core())
                {
                    //Global Denominator
                    flex_dma_async_reduction(
                        info->L1_l/*dst_offset*/,
                        info->L1_l/*src_offset*/,
                        info->L1_l_size/*transfer_size*/,
                        COLLECTIVE_REDADD_FP_16/*fmt*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    flex_dma_async_broadcast(
                        info->L1_l/*dst_offset*/,
                        info->L1_l/*src_offset*/,
                        info->L1_l_size/*transfer_size*/,
                        info->group.wakeup_row_mask/*row_mask*/,
                        (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                    flex_dma_async_wait_all(); // Wait for iDMA Finishing
                }


                /************************/
                /*                      */
                /*      Phase 4-4       */
                /*      B1_SV_4         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S1, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                    //S = S/l
                    MLA_vector_M_div_V(
                        info->Br_s,
                        info->Bc_s,
                        info->L1_S2,
                        info->L1_l
                        );
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 0 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O1/*dst_offset*/,
                            info->L1_O1/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDADD_FP_16/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->O_tile_tmp_base_batch_1 + 2 * info->d * info->elem_size, /*destination*/
                            info->L1_O1, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->O_head_dim, /*destination stride*/
                            info->d * info->elem_size, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 5-1       */
                /*      B2_SV_1         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S2, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 1 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O2/*dst_offset*/,
                            info->L1_O2/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDADD_FP_16/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->O_tile_tmp_base_batch_1 + 3 * info->d * info->elem_size, /*destination*/
                            info->L1_O2, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->O_head_dim, /*destination stride*/
                            info->d * info->elem_size, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 5-2       */
                /*      B2_SV_2         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S2, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V1, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 2 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V1/*dst_offset*/,
                            info->L1_V1/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O1/*dst_offset*/,
                            info->L1_O1/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDADD_FP_16/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->O_tile_tmp_base_batch_2 + 0 * info->d * info->elem_size, /*destination*/
                            info->L1_O1, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->O_head_dim, /*destination stride*/
                            info->d * info->elem_size, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 5-3       */
                /*      B2_SV_3         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S2, info->L1_V1, info->L1_O1, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_y == 0)
                    {
                        //load from south edge
                        flex_dma_async_2d(
                            info->L1_V2, /*destination*/
                            info->C_tile_tmp_base_batch_2 + 3 * info->d * info->elem_size, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->d * info->elem_size, /*destination stride*/
                            info->meta_head_dim, /*source stride*/
                            info->Bc_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //col-wise multicast
                        flex_dma_async_broadcast(
                            info->L1_V2/*dst_offset*/,
                            info->L1_V2/*src_offset*/,
                            info->L1_V_size/*transfer_size*/,
                            (ARCH_NUM_CLUSTER_X - 1)/*row_mask*/,
                            info->group.wakeup_col_mask/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O2/*dst_offset*/,
                            info->L1_O2/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDADD_FP_16/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->O_tile_tmp_base_batch_2 + 1 * info->d * info->elem_size, /*destination*/
                            info->L1_O2, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->O_head_dim, /*destination stride*/
                            info->d * info->elem_size, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }


                /************************/
                /*                      */
                /*      Phase 5-4       */
                /*      B2_SV_4         */
                /*                      */
                /************************/

                grid_sync_group_barrier_xy(&(info->group));
                if (flex_is_first_core())
                {
                    flex_redmule_wait();
                }
                flex_intra_cluster_sync();
                if (flex_is_first_core())
                {
                    flex_redmule_trigger(info->L1_S2, info->L1_V2, info->L1_O2, REDMULE_COMPUTE_TYPE);
                }
                if (flex_is_dm_core())
                {
                    if (info->cluster_in_group_id_x == 0)
                    {
                        //Reduction O
                        flex_dma_async_reduction(
                            info->L1_O1/*dst_offset*/,
                            info->L1_O1/*src_offset*/,
                            info->L1_O_size/*transfer_size*/,
                            COLLECTIVE_REDADD_FP_16/*fmt*/,
                            info->group.wakeup_row_mask/*row_mask*/,
                            (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                        //Store O
                        flex_dma_async_2d(
                            info->O_tile_tmp_base_batch_2 + 2 * info->d * info->elem_size, /*destination*/
                            info->L1_O1, /*source*/
                            info->d * info->elem_size, /*transfer size*/
                            info->O_head_dim, /*destination stride*/
                            info->d * info->elem_size, /*source stride*/
                            info->Br_s /*repeat*/); //Start 2D iDMA
                        flex_dma_async_wait_all(); // Wait for iDMA Finishing
                    }
                }

                //Record Store Address
                recorded_store = 1;
                recorded_store_addr = info->O_tile_tmp_base_batch_2 + 3 * info->d * info->elem_size;
            }
        }
    }

    /************************/
    /*                      */
    /*      Phase 6-1       */
    /*      Last Store      */
    /*                      */
    /************************/
    grid_sync_group_barrier_xy(&(info->group));
    if (flex_is_first_core())
    {
        flex_redmule_wait();
    }
    flex_intra_cluster_sync();
    if (flex_is_dm_core())
    {
        if (info->cluster_in_group_id_x == 0)
        {
            //Reduction O
            flex_dma_async_reduction(
                info->L1_O2/*dst_offset*/,
                info->L1_O2/*src_offset*/,
                info->L1_O_size/*transfer_size*/,
                COLLECTIVE_REDADD_FP_16/*fmt*/,
                info->group.wakeup_row_mask/*row_mask*/,
                (ARCH_NUM_CLUSTER_Y - 1)/*col_mask*/);
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
            //Store O
            flex_dma_async_2d(
                info->O_tile_tmp_base_batch_2 + 3 * info->d * info->elem_size, /*destination*/
                info->L1_O2, /*source*/
                info->d * info->elem_size, /*transfer size*/
                info->O_head_dim, /*destination stride*/
                info->d * info->elem_size, /*source stride*/
                info->Br_s /*repeat*/); //Start 2D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
    }
    grid_sync_group_barrier_xy(&(info->group));

}

#endif  