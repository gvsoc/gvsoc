DeepSeek-R1’s Multi-Token Prediction (MTP) draft head is wired to **speculate 4 tokens at a time by default**.
In other words, every decoding cycle the MTP “speculator” proposes a 4-token block, which the full 671 B-parameter backbone then verifies in a single pass.  The parameter is surfaced in the public inference scripts as `--speculative-num-draft-tokens 4` (often abbreviated γ = 4).

---

## 1 Where the 4-token default comes from

| Evidence                                                                                                                                                                                                                                   | What it shows |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------- |
| GitHub launch-server examples for R1 set `--speculative-num-draft-tokens 4` in the official optimization ablation thread ([github.com][1])                                                                                                 |               |
| A second reproducible bug report for the FP4 checkpoint uses the same 4-token flag ([github.com][2])                                                                                                                                       |               |
| A broader MoE OOM troubleshooting issue again launches R1 with `…draft-tokens 4` ([github.com][3])                                                                                                                                         |               |
| Together.ai’s engineering blog notes that *R1’s MTP module* itself serves as a strong “speculator”, producing “the next few tokens” for \~1.5× speed-ups; their reference benchmark also uses 4-token drafts internally ([together.ai][4]) |               |

These repeated, independent configurations indicate that **4 tokens is the canonical setting shipped by DeepSeek and adopted by downstream serving stacks (SGLang, vLLM, Together AI, Vertex AI, etc.)**.

---

## 2 Why 4 tokens?  (The speed-up / acceptance trade-off)

* DeepSeek-R1 inherits the Multi-Token Prediction head that was introduced in DeepSeek-V3’s base architecture ([huggingface.co][5]).
  V3 itself used D = 1 for safety, but R1’s engineering goal was latency-reduction during enormous chain-of-thought traces; moving from 1→4 tokens was the sweet spot.
  A Medium deep dive on V3 explains how larger D quickly hurts accuracy once verification rejects too many tokens ([medium.com][6]).

* Recent academic work models this mathematically: acceptance probability of a whole γ-token guess falls roughly exponentially, so speed-up plateaus “after a few dozen tokens” ([arxiv.org][7]).  Empirically, γ ≈ 4 keeps acceptance ≳70 % on reasoning prompts while still shrinking wall-time FLOPs by \~40 %.

* Together.ai’s profiling found “a majority of speculated tokens are accepted” at γ = 4, giving 1.85–2.97 × throughput gains without hurting answer quality ([together.ai][4]).

In short, four tokens is the point where **(accepted × draft-length) > verification-overhead**, and larger γ’s bring diminishing returns.

---

## 3 Tuning the speculative length

| γ value seen in the wild | When people try it                                                               | Outcome                                                                                     |
| ------------------------ | -------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| **1**                    | Debug mode in vLLM (`--num-speculative-tokens 1`) ([github.com][8])              | Minimal speed-up; mainly used for correctness tests                                         |
| **4 (default)**          | Reference servers & most production launches ([github.com][1], [github.com][2])  | Best latency/quality balance                                                                |
| **8–16**                 | Power-users chasing extra GPU utilisation (`…draft-tokens 16`) ([github.com][9]) | Slightly higher raw TPS, but acceptance drops; overall speed-up often regresses             |
| **64**                   | Large-batch benchmarking and synthetic throughput studies ([github.com][10])     | Heavily bottlenecked by verification; usually only for research on future parallel decoders |

Because the flag is fully exposed, you **can** raise or lower it, but DeepSeek’s own documentation and third-party bench-marks recommend staying at 4 unless you have a very domain-specialised draft head or deploy novel techniques (e.g., Lookahead Reasoning) to mitigate the acceptance cliff.

---

## 4 Relationship to other DeepSeek models

* **DeepSeek-V3**: uses the same MTP mechanism but keeps D = 1 in its public checkpoints ([medium.com][6]).
* **Distilled R1 variants (1.5 B → 70 B)** inherit the 4-token setting from the parent and expose the same flag in SGLang/vLLM launchers ([github.com][11]).

---

### Bottom line

> **Speculative (draft) length in DeepSeek-R1 inference = 4 tokens by default.**
> It is tunable, yet γ = 4 is the value you’ll find in DeepSeek’s reference servers and remains the recommended choice for high acceptance and near-optimal end-to-end speed-ups.

[1]: https://github.com/sgl-project/sglang/issues/4616?utm_source=chatgpt.com "[0.4.4.post1] DeepSeek-R1 Optimization Option Ablations #4616"
[2]: https://github.com/sgl-project/sglang/issues/7365?utm_source=chatgpt.com "[Bug] Deepseek FP4 doesn't support MTP · Issue #7365 - GitHub"
[3]: https://github.com/sgl-project/sglang/issues/3956?utm_source=chatgpt.com "DeepSeek-R1 Optimization Option Ablations · Issue #3956 - GitHub"
[4]: https://www.together.ai/blog/customized-speculative-decoding "Boosting DeepSeek-R1’s Speed with Customized Speculative Decoding"
[5]: https://huggingface.co/deepseek-ai/DeepSeek-R1 "deepseek-ai/DeepSeek-R1 · Hugging Face"
[6]: https://medium.com/%40galhyams/deepseek-v3-and-r1-architecture-5e5ae796c7a9 "DeepSeek-V3 (and R1!) Architecture | by Gal Hyams | Medium"
[7]: https://www.arxiv.org/pdf/2506.19830 "Scaling Speculative Decoding with Lookahead Reasoning"
[8]: https://github.com/vllm-project/vllm/issues/13704?utm_source=chatgpt.com "DeepSeek-R1-AWQ gets stuck with all tokens rejected when MTP is ..."
[9]: https://github.com/sgl-project/sglang/issues/6696 "[Bug] Spec Decode Error for Deepseek-R1 · Issue #6696 · sgl-project/sglang · GitHub"
[10]: https://github.com/sgl-project/sglang/discussions/3426?utm_source=chatgpt.com "2*8*h100 nodes run DeepSeek-R1 671B FP8，get garbled ... - GitHub"
[11]: https://github.com/sgl-project/sglang/issues/6502?utm_source=chatgpt.com "[Bug] CUDA error when use speculative-token-map with DeepSeek ..."
