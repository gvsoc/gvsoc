#ifndef _SPATZ_GEMM_CONFIG_H_
#define _SPATZ_GEMM_CONFIG_H_

#define GEMM_M_SIZE ((uint64_t)64)
#define GEMM_N_SIZE ((uint64_t)4096)
#define GEMM_K_SIZE ((uint64_t)22016)
#define GEMM_M_TILE ((uint64_t)64)
#define GEMM_N_TILE ((uint64_t)512)
#define GEMM_K_TILE ((uint64_t)64)
#define GEMM_SUMMA_SCALE_X ((uint64_t)4)
#define GEMM_SUMMA_SCALE_Y ((uint64_t)1)
#define GEMM_SUMMA_GROUP_NUMBER ((uint64_t)4)
#define GEMM_SUMMA_GROUP_REDUCE ((uint64_t)1)
#define GEMM_SUMMA_GROUP_SPLITK ((uint64_t)1)
#define GEMM_SUMMA_GROUP_SPLITN ((uint64_t)0)
#define GEMM_SUMMA_GROUP_GAP_X ((uint64_t)11008)
#define GEMM_SUMMA_GROUP_GAP_W ((uint64_t)45088768)
#define GEMM_SUMMA_GROUP_GAP_Z ((uint64_t)0)
#define DATA_TYPE_WIDTH 16
#define DATA_TYPE_BYTE 2
#define GEMM_FP16
#define REDMULE_COMPUTE_TYPE REDMULE_NONE_16
#define COLLECTIVE_REDSUM_TYPE COLLECTIVE_REDADD_NONE
#define COLLECTIVE_REDMAX_TYPE COLLECTIVE_REDMAX_NONE
#define STR(x) #x
#define XSTR(x) STR(x)

#endif // _SPATZ_GEMM_CONFIG_H_
