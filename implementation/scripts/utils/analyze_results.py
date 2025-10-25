#
# Copyright (C) 2025 ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Author: Chi Zhang <chizhang@ethz.ch>
import os
import io
import re
import math
import numpy as np
import matplotlib.pyplot as plt

def plot_runtime_breakdown_and_utilization_curve(results, save_path, unit = '', scale_div = 1):
    runtime = []
    util = []
    kernels = []

    # Collect Data
    for kernel in results:
        runtime.append(results[kernel]['runtime'] / scale_div)
        util.append(results[kernel]['redmule_uti'] * 100)
        kernels.append(kernel)
        pass

    runtime = np.array(runtime)
    util = np.array(util)

    total_ms = runtime.sum()

    # Convert to percentage
    runtime_pct = runtime / total_ms * 100
    starts_pct = np.concatenate([[0], np.cumsum(runtime_pct)[:-1]])

    fig, ax1 = plt.subplots(figsize=(15, 2.5))

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

    plt.title(f'Runtime Breakdown with Utilisation Curve')
    plt.savefig(save_path, bbox_inches="tight")
    pass

def plot_kernel_roofline(arch, raw_results, save_path, *, title=None, annotate=False):
    peak_perf = 2 * arch.num_cluster_x * arch.num_cluster_y * arch.redmule_ce_height * arch.redmule_ce_width / 1000 #TFLOPs
    bandwidth = 64 * sum(arch.hbm_chan_placement) #GB/s
    results = {}
    for k, v in raw_results.items():
        if "achieved_flop_per_cycle" in v and "arithmetic_intensity" in v:
            results[k] = {"TFLOPS": v['achieved_flop_per_cycle'] / 1000, "AI": v['arithmetic_intensity']}
            pass
        pass
    """
    Plot a Roofline model for a single device.

    Parameters
    ----------
    peak_perf : float
        Peak compute performance in TFLOPS (e.g., 125 for 125 TFLOPS).
    bandwidth : float
        Peak memory bandwidth in GB/s (decimal, 1 GB = 1e9 bytes).
    results : dict
        Mapping from kernel name to a dict with keys:
            - "TFLOPS": achieved throughput in TFLOPS
            - "AI": arithmetic intensity in FLOPs/byte
        Example:
            {
                "gemm": {"TFLOPS": 1889, "AI": 788},
                "gemv": {"TFLOPS": 100,  "AI": 0.66},
            }
    title : str, optional
        Plot title. Defaults to a generated one.
    annotate : bool, optional
        If True, label each kernel point with its name.

    Returns
    -------
    (fig, ax) : matplotlib Figure and Axes
    """
    # --- derived quantities ---
    # memory roofline: y = (bandwidth * AI) / 1000  (=> TFLOPS; since 1e9/1e12 = 1/1000)
    mem_slope = bandwidth / 1000.0  # TFLOPS per (FLOPs/byte)

    # knee (intersection) AI where memory equals compute
    # mem_slope * AI_knee = peak_perf  =>  AI_knee = peak_perf / mem_slope
    AI_knee = peak_perf / mem_slope if mem_slope > 0 else float('inf')

    # choose x/y ranges based on data and knee
    AIs = [max(1e-6, d["AI"]) for d in results.values()] if results else [1e-3, 1e3]
    y_vals = [max(1e-6, d["TFLOPS"]) for d in results.values()] if results else [1e-3, peak_perf]

    x_min = 10 ** math.floor(math.log10(min(AIs + [AI_knee * 0.2])))
    x_max = 10 ** math.ceil (math.log10(max(AIs + [AI_knee * 5.0])))
    y_min = 10 ** math.floor(math.log10(min(y_vals + [mem_slope * x_min, 0.1])))
    y_max = 10 ** math.ceil (math.log10(max(y_vals + [peak_perf, mem_slope * x_max])))

    fig, ax = plt.subplots(figsize=(7.5, 5.5))

    # --- rooflines ---
    # memory roof: y = mem_slope * x
    xs = [x_min, x_max]
    ys_mem = [mem_slope * x for x in xs]
    ax.plot(xs, ys_mem, linestyle='--', linewidth=1.5, label=f"Memory roof: {bandwidth:g} GB/s")

    # compute roof: y = peak_perf (horizontal)
    ax.plot(xs, [peak_perf, peak_perf], linestyle='--', linewidth=1.5, label=f"Compute roof: {peak_perf:g} TFLOPS")

    # --- knee marker ---
    if AI_knee > 0 and math.isfinite(AI_knee):
        ax.plot([AI_knee], [peak_perf], marker='o')
        ax.text(AI_knee, peak_perf, "  knee", va='bottom', ha='left', fontsize=9)

    # --- plot kernels ---
    for name, d in results.items():
        ai = max(1e-12, float(d["AI"]))
        tflops = max(1e-12, float(d["TFLOPS"]))
        ax.scatter([ai], [tflops], s=50, marker='o', label=name)
        if annotate:
            # small offset in log space for readability
            ax.text(ai * 1.05, tflops * 1.05, name, fontsize=9)

    # --- cosmetics ---
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlim(x_min, x_max)
    ax.set_ylim(y_min, y_max)
    ax.set_xlabel("Arithmetic Intensity (FLOPs/byte)")
    ax.set_ylabel("Performance (TFLOPS)")
    if title is None:
        title = f"Roofline (Peak: {peak_perf:g} TFLOPS, BW: {bandwidth:g} GB/s)"
    ax.set_title(title)
    ax.grid(True, which='both', linewidth=0.6, alpha=0.5)
    ax.legend(loc='upper left', fontsize=9, ncol=1, framealpha=0.9)

    # helpful tick marks near the knee
    for decade in [1, 10, 100, 1000, 10000, 100000]:
        if x_min <= decade <= x_max:
            ax.axvline(decade, linewidth=0.25, alpha=0.2)

    plt.tight_layout()
    plt.savefig(save_path, bbox_inches="tight")
    pass

def generate_polts(arch, results, save_root):
    # Create Save Path
    save_dir = save_root / "plots"
    os.system(f"mkdir -p {save_dir}")

    # Generate Runtime Breakdown
    save_path = save_dir / "runtime_breakdown_and_utilization_curve.pdf"
    plot_runtime_breakdown_and_utilization_curve(results, save_path, scale_div = 1000)

    # Generate Roofline Plot
    save_path = save_dir / "roofline.pdf"
    plot_kernel_roofline(arch, results, save_path)
    pass
