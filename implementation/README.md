# Kernel & End-to-End Dataflow Implementation for Modern LLMs

## 🚀 Getting Started with Kernel Execution

### ⚙️ Supported Kernels and Short Names

| Kernel Description                  | Short Name |
| ----------------------------------- | ---------- |
| 🪞 FlatAttention (MHA, MQA, GQA)    | `attn`     |
| 🔢 GEMM with SUMMA Dataflow         | `gemm`     |
| 🌈 RMSNorm                          | `norm`     |
| 🌀 RoPE (Rotary Position Embedding) | `rope`     |
| ✨ Activation (Sigmoid, ReLU, SiLU) | `acti`     |
| 📊 MoEGate (TopK Val&Idx of Score)  | `moeg`     |
| 🧩 MoEDispatch                      | `moed`     |

---

### 🏗️ Build, Preload, and Run Simulation

To build configuration, prepare preload data, and run simulation, use:

```bash
make <Kernel>-run
```

---

### ✅ Check Numerical Correctness

To verify numerical correctness, use:

```bash
make <Kernel>-num
```
