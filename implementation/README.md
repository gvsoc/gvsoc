# Kernel for End-to-End Dataflow Implementation for Modern LLMs

## 🚀 Getting Started with Kernel Execution

### ⚙️ Supported Kernels and Short Names

| Kernel Description                  | Short Name |
| ----------------------------------- | ---------- |
| 🪞 FlatAttention (MHA, MQA, GQA)    | `attn`     |
| 🔢 GEMM with SUMMA Dataflow         | `gemm`     |
| 🌈 RMSNorm                          | `norm`     |
| ✨ Activation (Sigmoid, ReLU, SiLU) | `acti`     |


---

### 🏗️ Build, Preload, and Run Simulation

To build configuration, prepare preload data, and run simulation, use:

```bash
make <Kernel>-run
```
The defualt kernel configuration are located in `config/kernels`
