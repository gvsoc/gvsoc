import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch
from matplotlib.lines import Line2D

# Data
runtime = np.array([
14336,
40140,
13212,
239361,
51109,
1497544,
263617,
14336,
96590
])  # ms
util = np.array([
0.00,
71.43,
6.78,
82.14,
48.09,
74.40,
87.01,
0.00,
3.71
])      # %
kernels = [f'K{i+1}' for i in range(len(runtime))]

starts = np.concatenate([[0], np.cumsum(runtime)[:-1]])

fig, ax1 = plt.subplots(figsize=(10, 3))

# Draw horizontal stacked bars
bar_patches = []
for s, w, k in zip(starts, runtime, kernels):
    p = ax1.barh(0, w, left=s, label=k)
    bar_patches.append(p[0])

ax1.set_yticks([])
ax1.set_xlabel('Runtime (ms)')
ax1.set_xlim(0, runtime.sum()*1.05)

# Utilisation step line
ax2 = ax1.twinx()
x_step = []
y_step = []
for s, w, u in zip(starts, runtime, util):
    x_step += [s, s + w]
    y_step += [u, u]
line = ax2.step(x_step, y_step, where='post', marker='o', color='black', label='Utilisation')[0]
ax2.set_ylabel('Compute Utilisation (%)')
ax2.set_ylim(0, 100)

# Build combined legend: kernel bars + utilisation line
handles = bar_patches + [line]
labels = kernels + ['Utilisation']
ax1.legend(handles, labels, bbox_to_anchor=(1.05, 1), loc='upper left', frameon=False)

plt.title('GPU Kernel Timeline with Utilisation Curve')
plt.tight_layout()
plt.show()
