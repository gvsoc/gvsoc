#ifndef _DEEPSEEKV3_CONFIG_H_
#define _DEEPSEEKV3_CONFIG_H_

#define DATA_TYPE_WIDTH             16
#define DATA_TYPE_BYTE              2
#define REDMULE_COMPUTE_TYPE        REDMULE_NONE_16
typedef uint16_t                    deepseek_data_t;
#define COLLECTIVE_REDSUM_TYPE      COLLECTIVE_REDADD_FP_16
#define COLLECTIVE_REDMAX_TYPE      COLLECTIVE_REDMAX_FP_16
#define DSV3_NUM_LAYERS ((uint64_t)61)
#define DSV3_EMBEDED_LENGTH ((uint64_t)7186)
#define DSV3_MAX_SEQUENCE_LENGTH ((uint64_t)8192)
#define DSV3_NUM_HEADS ((uint64_t)128)
#define DSV3_HEAD_DIMENSION ((uint64_t)128)
#define DSV3_Q_LORA_RANK ((uint64_t)1536)
#define DSV3_KV_LORA_RANK ((uint64_t)512)
#define DSV3_QK_NOPE_HEAD_DIM ((uint64_t)128)
#define DSV3_QK_ROPE_HEAD_DIM ((uint64_t)64)
#define DSV3_V_HEAD_DIM ((uint64_t)128 )
#define DSV3_MOE_INTER_DIM ((uint64_t)2048)
#define DSV3_N_ROUTED_EXPERTS ((uint64_t)256)
#define DSV3_N_SHARED_EXPERTS ((uint64_t)1)
#define DSV3_N_ACTIVATED_EXPERTS ((uint64_t)8)

#endif // _DEEPSEEKV3_CONFIG_H_
