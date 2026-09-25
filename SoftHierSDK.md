# Using the SoftHier SDK with GVSoC

`soft_hier_sdk/` supplies the runtime, applications, architecture configurations
and benchmark scripts for the `pulp.chips.soft_hier_old.flex_cluster` model.
The root Makefile already includes its `softhier_old.mk` integration.

Run the commands below from the **GVSoC repository root**, in Bash. Commands
using `make -C` select the SDK's implementation directory explicitly.

## 1. Initialize the environment

Use the host compiler and system dependencies described in the
[main README](README.md) and [DRAMSys guide](DRAMSys.md). The prepared workspace
uses Python 3.12; `python3` below must resolve to an interpreter compatible with
`core/requirements.txt`. Initialize the pinned submodules and a Python environment:

```bash
git submodule update --init --recursive
python3 -m venv build/soft_hier_venv
source build/soft_hier_venv/bin/activate
python3 -m pip install -r core/requirements.txt -r gapy/requirements.txt numpy tqdm

source sourceme.sh
source sourceme_systemc.sh
make dramsys_preparation
source soft_hier_sdk/sourceme.sh
export SOFTHIER_OLD_PYTHON=python3
```

`dramsys_preparation` prepares SystemC, the DRAMSys library and memory
configurations. The SDK environment installs its RISC-V toolchain under
`soft_hier_sdk/toolchain/` when needed and adds it to `PATH`. Confirm that
`command -v riscv32-unknown-elf-gcc` resolves before building software.

In a later shell, reactivate the Python environment and source the three
environment scripts again. Keep the submodule revisions pinned by GVSoC so the
core, chip models and SDK remain compatible.

## 2. Build and run an application

The default application is `soft_hier_sdk/runtime/app_example`, using
`soft_hier_sdk/examples/SoftHier/config/arch_NoC1024.py` and fast cores:

```bash
make sh-old-hs
make sh-old-runv
```

The executable and disassembly are written to
`soft_hier_sdk/sw_build/softhier.elf` and `softhier.dump`. The verbose run saves
`soft_hier_sdk/sw_build/analyze_trace.txt`.

To select an application explicitly, for example the annotation API test:

```bash
make sh-old-hs \
  cfg=soft_hier_sdk/examples/SoftHier/config/arch_NoC1024.py \
  app=soft_hier_sdk/examples/SoftHier/software/annotation_test
make sh-old-runv core_model=fast
rg 'ANNOTATION_TEST_(PASS|FAIL)' soft_hier_sdk/sw_build/analyze_trace.txt
```

The annotation test prints `ANNOTATION_TEST_PASS` on success. Check the
application's result messages as well as simulator termination.

| Target | Action |
| --- | --- |
| `sh-old-config` | Copy the architecture into the model and generate runtime headers. |
| `sh-old-hw` | Generate the configuration, then build and install the SoftHier model. |
| `sh-old-sw` | Generate the configuration, then build the application. |
| `sh-old-hs` | Build both model and application. |
| `sh-old-run` | Run the existing executable with a focused RedMule trace. |
| `sh-old-runv` | Run with RedMule, iDMA and cluster-register traces. |
| `sh-old-pfto` | Convert the verbose trace to Perfetto JSON. |

| Setting | Use |
| --- | --- |
| `cfg=path/to/arch.py` | Architecture class used during configuration and build. |
| `app=path/to/application` | Application directory containing `CMakeLists.txt`. |
| `core_model=fast` or `accurate` | Core model used for simulation; default is `fast`. |
| `pld=path/to/preload.elf` | Optional preload ELF passed to the run target. |
| `SOFTHIER_OLD_SW_BUILD=path` | Software output directory; pass the same path to build and run. |
| `SOFTHIER_OLD_PYTHON=python3` | Python interpreter for architecture generation and trace conversion. |
| `CMAKE_FLAGS='-j 8'` | GVSoC hardware build parallelism. |

An architecture change requires rebuilding hardware and software with the same
`cfg`. Run targets use the installed model and existing executable; passing
`cfg` only to a run target does not rebuild them. Build and run different
architectures sequentially because they share the installed model and generated
runtime headers. `sh-old-sw` recreates its selected software output directory.

For a new application, use the SDK's
[app example](soft_hier_sdk/runtime/app_example) as the starting point and build
it with `app=...`.

## 3. Run implementation kernels

The implementation Makefile handles configuration, preload generation, building
and simulation for `attn`, `gemm`, `norm` and `acti`. For example:

```bash
make -C soft_hier_sdk/implementation norm-runv
```

Architecture files are under `implementation/config/arch/`; kernel parameters
are under `implementation/config/kernels/`. See the
[implementation README](soft_hier_sdk/implementation/README.md) for the kernel
flows and the [SDK README](soft_hier_sdk/README.md) for other applications.

### Sparse benchmark prerequisites

The `sparse` and `sparse_attn_access` wrappers use the prepared Sparse-DMA
calibration environment. In addition to the SDK setup above, they require:

```text
build/reproduce_dramsys/env.sh
build/sparse_dma/dram/dramsys_configs/HBM2E-3600.json
build/sparse_dma/dramsys-rtl/build/lib/libDRAMSys_Simulator.so
```

These are workspace-generated files, not files supplied by a fresh Git checkout.
`soft_hier_sdk/implementation/scripts/sparse_dma/env.sh` loads that environment
and selects the matching HBM2E configuration and library for both benchmarks.
The current workspace environment also selects its own Python and compiler
paths; those tools and packages must be available on the host.

For a new calibration workspace, follow the
[standalone Sparse-DMA setup](scripts/sparse_dma/README.md), including its
initialized sibling `spatz-sparse-dma` repository and reproduced RTL inputs.
Once that environment and the RTL inputs are ready, these steps prepare the
shared HBM resources:

```bash
bash scripts/sparse_dma/step.sh prepare
bash scripts/sparse_dma/step.sh build-dram
```

### Sparse DMA sanity test

The three cases are core loop, inlined loop and HW gather. Cluster 0 gathers
128 rows of 256 bytes through the NoC from west HBM node 0:

```bash
SOFTHIER_IMPL="$PWD/soft_hier_sdk/implementation"
make -C "$SOFTHIER_IMPL" sparse-runv \
  SOFTHIER_DATA_NOC=legacy \
  SPARSE_OUTPUT="$SOFTHIER_IMPL/build/sparse_dma_sdk_example"
```

The command prepares data, builds, runs all three cases and generates
`REPORT.md`, `results.csv`, `results.json`, `cycles.*` and `hbm_bandwidth.*`
in the selected output directory. Plot formats are PNG, SVG and PDF.
See the [Sparse DMA guide](soft_hier_sdk/implementation/sw/SparseDMA/README.md).

### Sparse attention access

Each cluster gathers selected tokens from its own independent context. Defaults
are FP16, 128 elements per token, 1,024 selected tokens from a 131,072-token
context, seed 42, sorted indices, and four west HBM nodes:

```bash
make -C "$SOFTHIER_IMPL" sparse_attn_access-run \
  SOFTHIER_DATA_NOC=legacy \
  SPARSE_ATTN_OUTPUT="$SOFTHIER_IMPL/build/sparse_attn_access_sdk_example"
```

For 16 HBM nodes on all four edges and 128 selected tokens from a 1,024-token
context:

```bash
make -C "$SOFTHIER_IMPL" sparse_attn_access-run \
  SOFTHIER_DATA_NOC=legacy \
  SPARSE_ATTN_ARCH_FILE="$SOFTHIER_IMPL/config/arch/sparse_attn_access_hbm16.py" \
  SPARSE_ATTN_KERNEL_FILE="$SOFTHIER_IMPL/config/kernels/sparse_attn_access_m1024_n128.py" \
  SPARSE_ATTN_OUTPUT="$SOFTHIER_IMPL/build/sparse_attn_access_hbm16_small_sdk_example"
```

Each run produces a report, runtime and HBM-utilization plots, per-node HBM
plots, and NoC traffic maps for all three cases. Application binaries, preloads
and simulation logs are saved per case. Choose a fresh output directory for
each experiment; sparse attention rejects overwriting saved runs.

Regenerate reports from saved runs by replacing `sparse-runv` with
`sparse-report`, or `sparse_attn_access-run` with `sparse_attn_access-report`,
and keeping the same output setting. The
[sparse attention guide](soft_hier_sdk/implementation/sw/SparseAttnAccess/README.md)
describes data placement, configuration parameters and measurement boundaries.

## 4. Select the data NoC

`legacy` is the default. For FlooNoC v2, replace `SOFTHIER_DATA_NOC=legacy` with
`SOFTHIER_DATA_NOC=floonoc_v2` in a benchmark command and select a new output
directory. The benchmark command applies the setting to both build and run.
For separate `sh-old-hw` and `sh-old-runv` steps, set the same value for both.

V2 supports unicast reads/writes with `hbm_node_aliase=1`; use `legacy` for DMA
collectives and HBM edge aliases. Its traffic plots show separate request and
read-data return networks. See the
[data NoC guide](pulp/pulp/chips/soft_hier_old/DATA_NOC.md) for configuration
fields and bridge timing assumptions.

## 5. Inspect traces and resolve setup issues

After a generic `sh-old-runv` run:

```bash
make sh-old-pfto
```

Open `soft_hier_sdk/sw_build/perfetto.json` in Perfetto. The intermediate
annotation intervals are saved in `soft_hier_sdk/sw_build/roi.json`. Sparse
benchmarks have their own reports and per-case traces under their output
directory.

| Symptom | Check |
| --- | --- |
| No `sh-old-*` make target | Initialize the SDK submodule and run from the GVSoC root. |
| RISC-V compiler not found | Source `soft_hier_sdk/sourceme.sh`; verify the SDK toolchain installation. |
| Python module missing | Activate the intended Python environment and install its requirements; sparse wrappers load `build/reproduce_dramsys/env.sh`. |
| DRAMSys/SystemC library or config missing | Complete DRAMSys preparation and source `sourceme_systemc.sh`; sparse benchmarks also need the HBM2E files listed above. |
| A changed architecture is not reflected in the run | Rebuild both hardware and software using the selected `cfg`. |
| Sparse attention says a run already exists | Set `SPARSE_ATTN_OUTPUT` to a fresh directory, or use the report target for the saved run. |
