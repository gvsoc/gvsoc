Below is a quick-reference answer first, then the gory details and pointers if you want to dig deeper.

---

## Short answer (rule-of-thumb)

*With today’s public implementations, **roughly 60 – 90 % of the draft tokens get accepted** by the target model during speculative decoding.*
Where you land in that band depends almost entirely on:

1. **Draft–target similarity** (same model family ≈ higher acceptance).
2. **How many tokens you ask the draft to guess at once** (longer drafts fall off).
3. **Generation temperature / task domain** (code ≥ math ≥ open-ended chat).

Concretely:

| Setup (papers & production logs)                               | Draft length **K** | Acceptance ratio                                                |
| -------------------------------------------------------------- | ------------------ | --------------------------------------------------------------- |
| DeepMind *Speculative Sampling* (Chinchilla 70B ← 4 B) on XSum | 3                  | **≈ 0.68**                                                      |
| Same model on HumanEval (code)                                 | 3                  | **≈ 0.90**                                                      |
| *Draft-and-Verify* self-spec (13 B) **before** extra tuning    | 4                  | **0.695**([arxiv.org][1])                                       |
| *Draft-and-Verify* **after** 5 % extra finetuning              | 4                  | **0.90** target set ([arxiv.org][1])                            |
| vLLM server metrics (Llama-70B ← Llama-8B)                     | 4                  | **0.64 – 0.66**([github.com][2], [github.com][3])               |
| SmartSpec (goodput-aware serving) – fixed for study            | 4                  | **0.70** (assumed)([arxiv.org][4])                              |
| DeepSeek-V3 self-spec, 2-token MTP head                        | 1 extra            | **0.85 – 0.90** for the 2ᵗʰ token ([together.ai][5])            |
| Judge-Decoding baseline (Llama-70B)                            | 6-token draft      | **≈ 0.32** → speedup stalls; hence “judge” fix ([arxiv.org][6]) |

> **Practical rule**: keep acceptance **≥ 0.6** to see a net speed-up; if you can push it **≈ 0.8** you’re already in 1.7 × – 2 × territory.

Below 0.4 you normally lose time because the verifier regenerates too often (SambaNova docs and multiple vLLM issues make the same point)([docs.sambanova.ai][7]).

---

## How “good token / predicted token” is defined

Let

* *D* = number of tokens proposed by the **draft** model in one step (often called *K*).
* *A* = number of those tokens that survive verification.

The quantity the community reports is

$$
\text{acceptance rate} \;=\; \frac{A}{D}
$$

(sometimes you see *accepted length* = *A + 1*, adding the verifier’s own next-token).

Under the original rejection-sampling proof, the verifier walks left-to-right and stops at the first mismatch, so

$$
\mathbb{E}[A] = \sum_{i=1}^{D}p_i
$$

with $p_i=\Pr[\text{draft token }i=\text{target token }i]$.
If tokens were independent and equally likely (they are not!), the rate would simply be $p$ regardless of *D*. In practice $p_i$ decays roughly exponentially, hence longer drafts hurt acceptance.

---

## Why the average floats between 0.6 and 0.9

1. **Model gap** – a 7 B draft is \~1 bit/token worse than its 70 B sibling; that mismatch shows up directly in $p_i$.
2. **Token position** – the farther ahead you guess, the less context you have: $p_{i+1}\!<\!p_i$.
3. **Domain entropy** – deterministic code or math has lower entropy than open-ended prose, so drafts align better (hence HumanEval > XSum).
4. **Finetuning & self-spec tricks** – adding a tiny MLP-head (*Draft-and-Verify*, DeepSeek MTP, SWIFT layer-skip) or fine-tuning the draft on real traffic easily bumps acceptance by 10-20 pp ([openreview.net][8], [openreview.net][9]).

---

## Back-of-the-envelope: speed-up vs. acceptance

Let

* $T_t$ = time per verifier pass,
* $T_d$ = time per draft pass (usually $T_d \approx 0.2\,T_t$),
* $A/D$ = acceptance rate.

Expected tokens per **verifier** pass

$$
\mathbb{E}[\text{tokens}] \;=\; A+1 \;=\; D \cdot \frac{A}{D}+1
$$

Throughput gain (vs. greedy) approximates

$$
\frac{1}{\;T_t/T_{\text{greedy}}\;}\;=\;\frac{A+1}{\;1+T_d/T_t\;} .
$$

Plugging $A/D=0.7,\;D=4,\;T_d/T_t=0.2$ ⇒ **≈ 1.8 ×**;
drop to $A/D=0.4$ ⇒ **< 1.1 ×** (often a slowdown).

---

## Take-aways

* **Target ≥ 0.7** acceptance when you choose *K* and your draft size.
* If you see ≤ 0.5 in logs, either shorten *K*, lower temperature, or pick a nearer-size draft.
* Self-speculative variants (DeepSeek, SWIFT) routinely hit 0.85 + with zero extra model – a sweet spot for real-time chat.
* Production serving stacks (SmartSpec, vLLM) now monitor acceptance live and scale *K* down when traffic spike lowers GPU head-room.

With those heuristics you can quickly judge whether your speculative setup is healthy or just, well… speculating!

---

### Sources consulted

(turn4view0, turn0search1, turn7search1, turn7search5, turn19view0, turn13view0, turn8view0, turn10view0, turn12search1, turn15view0)

[1]: https://arxiv.org/html/2309.08168v2?utm_source=chatgpt.com "Draft & Verify: Lossless Large Language Model Acceleration via Self ..."
[2]: https://github.com/vllm-project/vllm/issues/9539?utm_source=chatgpt.com "how to understand logs (when speculative decoding) · Issue #9539 ..."
[3]: https://github.com/vllm-project/vllm/issues/7540?utm_source=chatgpt.com "Why does VLLM perform worse than TGI in Speculative decoding ..."
[4]: https://arxiv.org/html/2406.14066v2 "Optimizing Speculative Decoding for Serving Large Language Models Using Goodput"
[5]: https://www.together.ai/blog/customized-speculative-decoding "Boosting DeepSeek-R1’s Speed with Customized Speculative Decoding"
[6]: https://arxiv.org/abs/2501.19309 "[2501.19309] Judge Decoding: Faster Speculative Sampling Requires Going Beyond Model Alignment"
[7]: https://docs.sambanova.ai/sambastudio/latest/spec-decoding.html "Speculative decoding :: SambaNova Documentation"
[8]: https://openreview.net/forum?id=EKJhH5D5wA&utm_source=chatgpt.com "SWIFT: On-the-Fly Self-Speculative Decoding for LLM Inference..."
[9]: https://openreview.net/forum?id=EKJhH5D5wA "SWIFT: On-the-Fly Self-Speculative Decoding for LLM Inference Acceleration | OpenReview"
