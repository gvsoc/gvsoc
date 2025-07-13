import matplotlib.pyplot as plt
import numpy as np

#Overview
batch_per_chip="4"
Toplopgy="EPO"
# Data
runtime = np.array([
1792,
17597,
3301,
101408,
17371,
282624,
119477,
1792,
87232,
51453,
0,
209788,
0
])  # us
util = np.array([
0.00,
2.55,
0.42,
3.03,
2.21,
6.16,
3.00,
0.00,
0.06,
2.63,
0.00,
5.16,
0.00
]) # %
kernels = [
"RMSNorm",
"Fused_Latent_QKT",
"Latent_KR",
"Proj_QC",
"Proj_QR",
"FlatMLA",
"Proj_O",
"AddNorm",
"MoE_Gate",
"MoE_Shared_Compute",
"MoE_Dispatch",
"MoE_Routed_Compute",
"MoE_Combine"
]

total_ms = runtime.sum()

# Convert to percentage
runtime_pct = runtime / total_ms * 100
starts_pct = np.concatenate([[0], np.cumsum(runtime_pct)[:-1]])

fig, ax1 = plt.subplots(figsize=(10, 2.5))

# Percentage stacked bar (horizontal)
bar_patches = []
for s, w, k in zip(starts_pct, runtime_pct, kernels):
    p = ax1.barh(0, w, left=s, label=k)
    bar_patches.append(p[0])

ax1.set_yticks([])
ax1.set_xlabel('Runtime (% of total execution time)')
ax1.set_xlim(0, 100)
ax1.set_ylim(-0.3, 0.3)

# Utilisation curve (step) mapped to % timeline
ax2 = ax1.twinx()
x_step = []
y_step = []
for s, w, u in zip(starts_pct, runtime_pct, util):
    x_step += [s, s + w]
    y_step += [u, u]
line = ax2.step(x_step, y_step, where='post', marker='o', color='black', label='Utilisation')[0]
ax2.set_ylabel('Compute Utilisation (%)')
ax2.set_ylim(0, 100)

# Legend (kernel segments + utilisation)
handles = bar_patches + [line]
labels = kernels + ['Utilisation']
ax1.legend(handles, labels, bbox_to_anchor=(1.05, 1), loc='upper left', frameon=False)

plt.title(f'Decoding Kernels Runtime Breakdown with Utilisation Curve (One Layer of DeepSeek-671B, Batch/Chip={batch_per_chip})')
plt.tight_layout()
# plt.savefig(f"plot/Decode_{Toplopgy}_B{batch_per_chip}.pdf", bbox_inches="tight")
# plt.savefig(f"plot/Decode_{Toplopgy}_B{batch_per_chip}.jpg")
plt.show()
