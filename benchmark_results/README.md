# Nvidia GH200 Benchmarking 
## FLOP breakdown

The following figures compare the floating-point operation breakdown of the attention mechanism and other computational kernels between the MHA+MLP model Qwen-chat-7B (*Qwen-7B*) and MLA+MoE models DeepSeek-v3-16B (*DSv3-16B*) and DeepSeek-v3-671B (*DSv3-671B*), during both the prefill and decode stages.
At long-context inference, the attention mechanism of *Qwen-7B* accounts for 19% of all floating-point operations, whereas in *DSv3-671B* this proportion increases to 71% during decoding, with a similar trend observed in the prefill stage.
 ![Tux, the Linux mascot](FlopBreakdownPrefill.png)
FLOP breakdown for LLM models during prefill

 ![Tux, the Linux mascot](FlopBreakdownDecode.png)
FLOP breakdown for LLM models during decode stages.

## Roofline Plot
Benchmarking of prefill and decode performance on Nvidia GH200 GPU. Evaluated with FP16 precision, varying head dimension (hd) and sequence length (sq) for prefill, while varying speculative length (sp) and kv cache length (kv) for decoding.
The following figures show the performance of FlashAttention-3 during the prefill stage and FlashMLA during decoding on the GH200 roofline model. Both implementations exhibit a large performance gap relative to the roofline, ranging from 26% to 64%.
### FlashAttention prefill performance
 ![Tux, the Linux mascot](GPU_not_good_FA.png)

### FlashMLA decode performance  
 ![Tux, the Linux mascot](GPU_not_good_FMLA.png)


## Attention Attention Dataflow Results (GH200)

The table below summarizes measured performance for FP16 attention kernels (FlashAttention-3, FlashMLA) across different configurations.

| Name                         | Throughput (TFLOPS) |
|------------------------------|---------------------|
| MHA_prefill_hd64_sq512       | 333.00 |
| MHA_prefill_hd64_sq1024      | 392.00 |
| MHA_prefill_hd64_sq2048      | 460.00 |
| MHA_prefill_hd64_sq4096      | 476.00 |
| MHA_prefill_hd128_sq1024     | 565.00 |
| MHA_prefill_hd128_sq2048     | 625.00 |
| MHA_decode_sp1_kv512         | 2.22 |
| MHA_decode_sp1_kv1024        | 2.46 |
| MHA_decode_sp1_kv2048        | 2.85 |
| MHA_decode_sp1_kv4096        | 3.13 |
| MHA_decode_sp2_kv512         | 4.37 |
| MHA_decode_sp2_kv1024        | 4.89 |
| MHA_decode_sp2_kv2048        | 5.70 |
| MHA_decode_sp2_kv4096        | 6.25 |
| MHA_decode_sp4_kv512         | 8.69 |
| MHA_decode_sp4_kv1024        | 9.69 |
| MHA_decode_sp4_kv2048        | 11.38 |
| GQA_decode_sp1_kv512         | 17.48 |
| GQA_decode_sp1_kv1024        | 23.99 |
| GQA_decode_sp1_kv2048        | 33.28 |
| GQA_decode_sp1_kv4096        | 41.16 |
| GQA_decode_sp2_kv512         | 34.78 |
| GQA_decode_sp2_kv1024        | 47.96 |
| GQA_decode_sp2_kv2048        | 66.26 |
| GQA_decode_sp2_kv4096        | 81.94 |
| GQA_decode_sp4_kv512         | 68.48 |
| GQA_decode_sp4_kv1024        | 94.01 |
| GQA_decode_sp4_kv2048        | 130.59 |
| GQA_decode_sp4_kv4096        | 162.39 |
| MLA_decode_sp2_kv128         | 156.00 |
| MLA_decode_sp2_kv256         | 258.00 |
| MLA_decode_sp2_kv512         | 378.00 |
| MLA_decode_sp2_kv1024        | 447.00 |
| MLA_decode_sp2_kv2048        | 470.00 |
| MLA_decode_sp2_kv4096        | 483.00 |
| MLA_decode_sp4_kv128         | 174.00 |
| MLA_decode_sp4_kv256         | 285.00 |
| MLA_decode_sp4_kv512         | 393.00 |
| MLA_decode_sp4_kv1024        | 473.00 |
| MLA_decode_sp4_kv2048        | 494.00 |
| MLA_decode_sp4_kv4096        | 506.00 |
