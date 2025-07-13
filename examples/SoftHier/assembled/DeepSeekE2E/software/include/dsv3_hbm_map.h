#ifndef _DSV3_HBM_MAP_H_
#define _DSV3_HBM_MAP_H_

#include <stdint.h>
#include "dsv3.h"

typedef struct DSv3HBMMapInfo
{
    //West Edge Address Mapping
    uint64_t                Input; // {(B * K)                                  * DSV3_EMBEDED_LENGTH                        }
    uint64_t                NormA; // {(B * K)                                  * DSV3_EMBEDED_LENGTH                        }
    uint64_t                LatnQ; // {(B * K)                                  * DSV3_Q_LORA_RANK                           }
    uint64_t                MetaQ; // {DSV3_NUM_HEADS                           * (DSV3_KV_LORA_RANK + DSV3_QK_ROPE_HEAD_DIM)} * B * K
    uint64_t                MLA1O; // {DSV3_NUM_HEADS                           * DSV3_KV_LORA_RANK                          } * B * K
    uint64_t                MLA2O; // {DSV3_NUM_HEADS                           * DSV3_HEAD_DIMENSION                        } * B * K
    uint64_t                MLA_O; // {(B * K)                                  * DSV3_EMBEDED_LENGTH                        }
    uint64_t                NormE; // {(B * K)                                  * DSV3_EMBEDED_LENGTH                        }
    uint64_t                GateS; // {(B * K)                                  * DSV3_N_ACTIVATED_EXPERTS                   }
    uint64_t                GateI; // {(B * K)                                  * DSV3_N_ACTIVATED_EXPERTS                   }
    uint64_t                MoEIt; // {(B * K)                                  * DSV3_MOE_INTER_DIM                         }
    uint64_t                MoESh; // {(B * K)                                  * DSV3_EMBEDED_LENGTH                        } * DSV3_N_SHARED_EXPERTS
    uint64_t                MoERt; // {(B * K)                                  * DSV3_EMBEDED_LENGTH                        } * DSV3_N_ROUTED_EXPERTS
    uint64_t                MoE_O; // {(B * K)                                  * DSV3_EMBEDED_LENGTH                        }
    uint64_t                AreaW;

    //South Edge Address Mapping
    uint64_t                KVCac; // {L                                        * (DSV3_KV_LORA_RANK + DSV3_QK_ROPE_HEAD_DIM)} * B
    uint64_t                RoPES; // {DSV3_MAX_SEQUENCE_LENGTH                 * DSV3_QK_ROPE_HEAD_DIM                      }
    uint64_t                RoPEC; // {DSV3_MAX_SEQUENCE_LENGTH                 * DSV3_QK_ROPE_HEAD_DIM                      }
    uint64_t                WDQKV; // {DSV3_EMBEDED_LENGTH                      * (DSV3_Q_LORA_RANK + DSV3_KV_LORA_RANK)     }
    uint64_t                W__KR; // {DSV3_EMBEDED_LENGTH                      * DSV3_QK_ROPE_HEAD_DIM                      }
    uint64_t                WUKQC; // {DSV3_Q_LORA_RANK                         * (DSV3_NUM_HEADS * DSV3_KV_LORA_RANK)       }
    uint64_t                WUKQR; // {DSV3_Q_LORA_RANK                         * (DSV3_NUM_HEADS * DSV3_QK_ROPE_HEAD_DIM)   }
    uint64_t                W__UV; // {DSV3_KV_LORA_RANK                        * DSV3_V_HEAD_DIM                            } * DSV3_NUM_HEADS
    uint64_t                W__AO; // {(DSV3_NUM_HEADS * DSV3_QK_ROPE_HEAD_DIM) * DSV3_EMBEDED_LENGTH                        }
    uint64_t                W__ER; // {DSV3_EMBEDED_LENGTH                      * DSV3_N_ROUTED_EXPERTS                      }
    uint64_t                WShaU; // {DSV3_EMBEDED_LENGTH                      * DSV3_MOE_INTER_DIM                         } * DSV3_N_SHARED_EXPERTS
    uint64_t                WShaG; // {DSV3_EMBEDED_LENGTH                      * DSV3_MOE_INTER_DIM                         } * DSV3_N_SHARED_EXPERTS
    uint64_t                WShaD; // {DSV3_MOE_INTER_DIM                       * DSV3_EMBEDED_LENGTH                        } * DSV3_N_SHARED_EXPERTS
    uint64_t                WRouU; // {DSV3_EMBEDED_LENGTH                      * DSV3_MOE_INTER_DIM                         } * DSV3_N_ROUTED_EXPERTS
    uint64_t                WRouG; // {DSV3_EMBEDED_LENGTH                      * DSV3_MOE_INTER_DIM                         } * DSV3_N_ROUTED_EXPERTS
    uint64_t                WRouD; // {DSV3_MOE_INTER_DIM                       * DSV3_EMBEDED_LENGTH                        } * DSV3_N_ROUTED_EXPERTS
    uint64_t                AreaS;

}DSv3HBMMapInfo;

DSv3HBMMapInfo DSv3HBMMapInfoAnalyze(
    uint64_t B/*Batch Size*/,
    uint64_t K/*Speculative Length*/,
    uint64_t L/*KV Cache Length*/)
{
    DSv3HBMMapInfo info;

    info.Input = hbm_west(0,0);
    info.NormA = info.Input + DATA_TYPE_BYTE * B * K * DSV3_EMBEDED_LENGTH;
    info.LatnQ = info.NormA + DATA_TYPE_BYTE * B * K * DSV3_EMBEDED_LENGTH;
    info.MetaQ = info.LatnQ + DATA_TYPE_BYTE * B * K * DSV3_Q_LORA_RANK;
    info.MLA1O = info.MetaQ + DATA_TYPE_BYTE * B * K * DSV3_NUM_HEADS * (DSV3_KV_LORA_RANK + DSV3_QK_ROPE_HEAD_DIM);
    info.MLA2O = info.MLA1O + DATA_TYPE_BYTE * B * K * DSV3_NUM_HEADS * DSV3_KV_LORA_RANK;
    info.MLA_O = info.MLA2O + DATA_TYPE_BYTE * B * K * DSV3_NUM_HEADS * DSV3_HEAD_DIMENSION;
    info.NormE = info.MLA_O + DATA_TYPE_BYTE * B * K * DSV3_EMBEDED_LENGTH;
    info.GateS = info.NormE + DATA_TYPE_BYTE * B * K * DSV3_EMBEDED_LENGTH;
    info.GateI = info.GateS + DATA_TYPE_BYTE * B * K * DSV3_N_ACTIVATED_EXPERTS;
    info.MoEIt = info.GateI + DATA_TYPE_BYTE * B * K * DSV3_N_ACTIVATED_EXPERTS;
    info.MoESh = info.MoEIt + DATA_TYPE_BYTE * B * K * DSV3_MOE_INTER_DIM;
    info.MoERt = info.MoESh + DATA_TYPE_BYTE * B * K * DSV3_EMBEDED_LENGTH * DSV3_N_SHARED_EXPERTS;
    info.MoE_O = info.MoERt + DATA_TYPE_BYTE * B * K * DSV3_EMBEDED_LENGTH * DSV3_N_ROUTED_EXPERTS;
    info.AreaW = info.MoE_O + DATA_TYPE_BYTE * B * K * DSV3_EMBEDED_LENGTH - info.Input;

    info.KVCac = hbm_south(0,0);
    info.RoPES = info.KVCac + DATA_TYPE_BYTE * B * L * (DSV3_KV_LORA_RANK + DSV3_QK_ROPE_HEAD_DIM);
    info.RoPEC = info.RoPES + DATA_TYPE_BYTE * DSV3_MAX_SEQUENCE_LENGTH * DSV3_QK_ROPE_HEAD_DIM;
    info.WDQKV = info.RoPEC + DATA_TYPE_BYTE * DSV3_MAX_SEQUENCE_LENGTH * DSV3_QK_ROPE_HEAD_DIM;
    info.W__KR = info.WDQKV + DATA_TYPE_BYTE * DSV3_EMBEDED_LENGTH * (DSV3_Q_LORA_RANK + DSV3_KV_LORA_RANK);
    info.WUKQC = info.W__KR + DATA_TYPE_BYTE * DSV3_EMBEDED_LENGTH * DSV3_QK_ROPE_HEAD_DIM;
    info.WUKQR = info.WUKQC + DATA_TYPE_BYTE * DSV3_Q_LORA_RANK * DSV3_NUM_HEADS * DSV3_KV_LORA_RANK;
    info.W__UV = info.WUKQR + DATA_TYPE_BYTE * DSV3_Q_LORA_RANK * DSV3_NUM_HEADS * DSV3_QK_ROPE_HEAD_DIM;
    info.W__AO = info.W__UV + DATA_TYPE_BYTE * DSV3_NUM_HEADS * DSV3_KV_LORA_RANK * DSV3_V_HEAD_DIM;
    info.W__ER = info.W__AO + DATA_TYPE_BYTE * DSV3_NUM_HEADS * DSV3_QK_ROPE_HEAD_DIM * DSV3_EMBEDED_LENGTH;
    info.WShaU = info.W__ER + DATA_TYPE_BYTE * DSV3_EMBEDED_LENGTH * DSV3_N_ROUTED_EXPERTS;
    info.WShaG = info.WShaU + DATA_TYPE_BYTE * DSV3_N_SHARED_EXPERTS * DSV3_EMBEDED_LENGTH * DSV3_MOE_INTER_DIM;
    info.WShaD = info.WShaG + DATA_TYPE_BYTE * DSV3_N_SHARED_EXPERTS * DSV3_EMBEDED_LENGTH * DSV3_MOE_INTER_DIM;
    info.WRouU = info.WShaD + DATA_TYPE_BYTE * DSV3_N_SHARED_EXPERTS * DSV3_MOE_INTER_DIM * DSV3_EMBEDED_LENGTH;
    info.WRouG = info.WRouU + DATA_TYPE_BYTE * DSV3_N_ROUTED_EXPERTS * DSV3_EMBEDED_LENGTH * DSV3_MOE_INTER_DIM;
    info.WRouD = info.WRouG + DATA_TYPE_BYTE * DSV3_N_ROUTED_EXPERTS * DSV3_EMBEDED_LENGTH * DSV3_MOE_INTER_DIM;
    info.AreaS = info.WRouD + DATA_TYPE_BYTE * DSV3_N_ROUTED_EXPERTS * DSV3_MOE_INTER_DIM * DSV3_EMBEDED_LENGTH - info.KVCac;

    return info;
}

void printDSv3HBMMapInfo(DSv3HBMMapInfo* info) {
    uint32_t H, L;

    printf("    West Edge Address Mapping\n");

    H = info->Input >> 32; L = (uint32_t)(info->Input);
    printf("        Input:                         0x%08x_%08x\n", H, L);

    H = info->NormA >> 32; L = (uint32_t)(info->NormA);
    printf("        NormA:                         0x%08x_%08x\n", H, L);

    H = info->LatnQ >> 32; L = (uint32_t)(info->LatnQ);
    printf("        LatnQ:                         0x%08x_%08x\n", H, L);

    H = info->MetaQ >> 32; L = (uint32_t)(info->MetaQ);
    printf("        MetaQ:                         0x%08x_%08x\n", H, L);

    H = info->MLA1O >> 32; L = (uint32_t)(info->MLA1O);
    printf("        MLA1O:                         0x%08x_%08x\n", H, L);

    H = info->MLA2O >> 32; L = (uint32_t)(info->MLA2O);
    printf("        MLA2O:                         0x%08x_%08x\n", H, L);

    H = info->MLA_O >> 32; L = (uint32_t)(info->MLA_O);
    printf("        MLA_O:                         0x%08x_%08x\n", H, L);

    H = info->NormE >> 32; L = (uint32_t)(info->NormE);
    printf("        NormE:                         0x%08x_%08x\n", H, L);

    H = info->GateS >> 32; L = (uint32_t)(info->GateS);
    printf("        GateS:                         0x%08x_%08x\n", H, L);

    H = info->GateI >> 32; L = (uint32_t)(info->GateI);
    printf("        GateI:                         0x%08x_%08x\n", H, L);

    H = info->MoEIt >> 32; L = (uint32_t)(info->MoEIt);
    printf("        MoEIt:                         0x%08x_%08x\n", H, L);

    H = info->MoESh >> 32; L = (uint32_t)(info->MoESh);
    printf("        MoESh:                         0x%08x_%08x\n", H, L);

    H = info->MoERt >> 32; L = (uint32_t)(info->MoERt);
    printf("        MoERt:                         0x%08x_%08x\n", H, L);

    H = info->MoE_O >> 32; L = (uint32_t)(info->MoE_O);
    printf("        MoE_O:                         0x%08x_%08x\n", H, L);

    H = info->AreaW >> 32; L = (uint32_t)(info->AreaW);
    printf("        AreaW:                         0x%08x_%08x\n", H, L);

    printf("    South Edge Address Mapping\n");

    H = info->KVCac >> 32; L = (uint32_t)(info->KVCac);
    printf("        KVCac:                         0x%08x_%08x\n", H, L);

    H = info->RoPES >> 32; L = (uint32_t)(info->RoPES);
    printf("        RoPES:                         0x%08x_%08x\n", H, L);

    H = info->RoPEC >> 32; L = (uint32_t)(info->RoPEC);
    printf("        RoPEC:                         0x%08x_%08x\n", H, L);

    H = info->WDQKV >> 32; L = (uint32_t)(info->WDQKV);
    printf("        WDQKV:                         0x%08x_%08x\n", H, L);

    H = info->W__KR >> 32; L = (uint32_t)(info->W__KR);
    printf("        W__KR:                         0x%08x_%08x\n", H, L);

    H = info->WUKQC >> 32; L = (uint32_t)(info->WUKQC);
    printf("        WUKQC:                         0x%08x_%08x\n", H, L);

    H = info->WUKQR >> 32; L = (uint32_t)(info->WUKQR);
    printf("        WUKQR:                         0x%08x_%08x\n", H, L);

    H = info->W__UV >> 32; L = (uint32_t)(info->W__UV);
    printf("        W__UV:                         0x%08x_%08x\n", H, L);

    H = info->W__AO >> 32; L = (uint32_t)(info->W__AO);
    printf("        W__AO:                         0x%08x_%08x\n", H, L);

    H = info->W__ER >> 32; L = (uint32_t)(info->W__ER);
    printf("        W__ER:                         0x%08x_%08x\n", H, L);

    H = info->WShaU >> 32; L = (uint32_t)(info->WShaU);
    printf("        WShaU:                         0x%08x_%08x\n", H, L);

    H = info->WShaG >> 32; L = (uint32_t)(info->WShaG);
    printf("        WShaG:                         0x%08x_%08x\n", H, L);

    H = info->WShaD >> 32; L = (uint32_t)(info->WShaD);
    printf("        WShaD:                         0x%08x_%08x\n", H, L);

    H = info->WRouU >> 32; L = (uint32_t)(info->WRouU);
    printf("        WRouU:                         0x%08x_%08x\n", H, L);

    H = info->WRouG >> 32; L = (uint32_t)(info->WRouG);
    printf("        WRouG:                         0x%08x_%08x\n", H, L);

    H = info->WRouD >> 32; L = (uint32_t)(info->WRouD);
    printf("        WRouD:                         0x%08x_%08x\n", H, L);

    H = info->AreaS >> 32; L = (uint32_t)(info->AreaS);
    printf("        AreaS:                         0x%08x_%08x\n", H, L);
}


#endif // _DSV3_HBM_MAP_H_