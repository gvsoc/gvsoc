#ifndef _MOEGATE_H_
#define _MOEGATE_H_

#include "flex_runtime.h"
#include "flex_printf.h"
#include "flex_redmule.h"
#include "flex_dma_pattern.h"
#include "flex_group_barrier.h"
#include "flex_libfp8.h"
#include "flex_libfp16.h"
#include "moeg.h"

typedef struct MoEGateInfo
{
    uint64_t        input_address;
    uint64_t        output_val_address;
    uint64_t        output_idx_address;
    uint32_t        num_total_token;
    uint32_t        num_routed_experts;
    uint32_t        num_active_experts;
    uint32_t        token_per_cluster;
    uint32_t        start_token;
    uint32_t        L1_I;
    uint32_t        L1_V;
    uint32_t        L1_D;
    uint32_t        L1_Area;
    uint32_t        L1_I_size;
    uint32_t        L1_V_size;
    uint32_t        L1_D_size;

    //L1 location for snitch partition
    uint32_t        L1P_I;
    uint32_t        L1P_V;
    uint32_t        L1P_D;
}MoEGateInfo;

MoEGateInfo MoEGateAnaylze(
    uint32_t num_total_token,
    uint32_t num_routed_experts,
    uint32_t num_active_experts,
    uint32_t token_per_cluster,
    uint64_t input_address,
    uint64_t output_val_address,
    uint64_t output_idx_address)
{
    MoEGateInfo info;

    info.input_address          = input_address;
    info.output_val_address     = output_val_address;
    info.output_idx_address     = output_idx_address;
    info.num_total_token        = num_total_token;
    info.num_routed_experts     = num_routed_experts;
    info.num_active_experts     = num_active_experts;
    info.token_per_cluster      = token_per_cluster;
    info.start_token            = flex_get_cluster_id() * info.token_per_cluster;
    info.L1_I                   = local(0);
    info.L1_V                   = info.L1_I + info.token_per_cluster * info.num_routed_experts * DATA_TYPE_BYTE;
    info.L1_D                   = info.L1_V + info.token_per_cluster * info.num_active_experts * DATA_TYPE_BYTE;
    info.L1_Area                = info.L1_D + info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.L1_I_size              = info.token_per_cluster * info.num_routed_experts * DATA_TYPE_BYTE;
    info.L1_V_size              = info.token_per_cluster * info.num_active_experts * DATA_TYPE_BYTE;
    info.L1_D_size              = info.token_per_cluster * info.num_active_experts * sizeof(int);
    info.L1P_I                  = info.L1_I + flex_get_core_id() * info.num_routed_experts * DATA_TYPE_BYTE;
    info.L1P_V                  = info.L1_V + flex_get_core_id() * info.num_active_experts * DATA_TYPE_BYTE;
    info.L1P_D                  = info.L1_D + flex_get_core_id() * info.num_active_experts * sizeof(int);

    return info;
}

void find_top_k(int N, int K, moeg_data_t arr[], moeg_data_t top_vals[], int top_idx[]);

void MoEGateRun(MoEGateInfo * info)
{
    while (info->start_token < info->num_total_token)
    {
        //Load gate score to L1 and initialize val & idx
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                info->L1_I, /*destination*/
                info->input_address + info->start_token * info->num_routed_experts * DATA_TYPE_BYTE, /*source*/
                info->L1_I_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_1d(info->L1_V,zomem(0),info->L1_V_size);
            flex_dma_async_1d(info->L1_D,zomem(0),info->L1_D_size);
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }
        flex_intra_cluster_sync();

        //Do MoEGate
        if (flex_get_core_id() < info->token_per_cluster)
        {
            find_top_k(
                info->num_routed_experts/*int N*/,
                info->num_active_experts/*int K*/,
                (moeg_data_t *)(info->L1P_I)/*moeg_data_t arr[]*/,
                (moeg_data_t *)(info->L1P_V)/*moeg_data_t top_vals[]*/,
                (int *)(info->L1P_D)/*int top_idx[]*/);
        }
        flex_intra_cluster_sync();

        //Store back to HBM
        if (flex_is_dm_core())
        {
            flex_dma_async_1d(
                info->output_val_address + info->start_token * info->num_active_experts * DATA_TYPE_BYTE, /*destination*/
                info->L1_V, /*source*/
                info->L1_V_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_1d(
                info->output_idx_address + info->start_token * info->num_active_experts * sizeof(int), /*destination*/
                info->L1_D, /*source*/
                info->L1_D_size/*transfer size*/); //Start 1D iDMA
            flex_dma_async_wait_all(); // Wait for iDMA Finishing
        }

        //Update counter
        info->start_token += ARCH_NUM_CLUSTER * info->token_per_cluster;
    }
}

inline int is_greater_than(moeg_data_t a, moeg_data_t b) {
    int cv;
    asm volatile(
        "fmv.h.x ft0, %[av]\n"   // move half value 'av' into ft0
        "fmv.h.x ft1, %[bv]\n"   // move half value 'bv' into ft1
        "flt.h   %[cv], ft1, ft0\n"   // ft1 less than ft0 (half-precision)
        : [cv] "=r"(cv)
        : [av] "r"(a), [bv] "r"(b)
        : "ft0", "ft1"    // clobbered registers
    );
    return cv;
}

void find_top_k(int N, int K, moeg_data_t arr[], moeg_data_t top_vals[], int top_idx[]) {

    for (int i = 0; i < N; i++) {
        moeg_data_t val = arr[i];

        // Check if val belongs in the top K
        if (is_greater_than(val, top_vals[K - 1])) {
            int j = K - 1;
            // Shift smaller elements down
            while (j > 0 && is_greater_than(val, top_vals[j - 1])) {
                top_vals[j] = top_vals[j - 1];
                top_idx[j]  = top_idx[j - 1];
                j--;
            }
            // Insert new element
            top_vals[j] = val;
            top_idx[j]  = i;
        }
    }
}

#endif