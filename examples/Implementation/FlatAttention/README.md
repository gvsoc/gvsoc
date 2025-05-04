# FlatAttention Source Code

This repository contains source code for the **FlatAttention** paper, which leverages the **SoftHier** platform for simulation and performance analysis.

## Folder Structure 📂

```plaintext
configs
├── arch.py
└── attn.py
scripts
├── assertion.py
├── check_results.py
├── gen_preload.py
└── sw_config.py
software
├── CMakeLists.txt
├── include
│   ├── FlatAttention.h
│   └── FlatAttentionUtil.h
└── main.c
```

- **configs/**
  - **arch.py**: Configuration for the SoftHier architecture.
  - **attn.py**: Configuration for the FlatAttention settings (also the primary controller for the workflow).
  
- **scripts/**
  - **assertion.py**: Sanity check for both SoftHier architecture and FlatAttention configurations.
  - **sw_config.py**: Passes configuration settings to the software.
  - **gen_preload.py**: Generates HBM preload data.
  - **check_results.py**: Checks numerical correctness after simulation.

- **software/**
  - **CMakeLists.txt**: Build configuration for the software.
  - **include/**
    - **FlatAttention.h** and **FlatAttentionUtil.h**: FlatAttention Implementation.
  - **main.c**: Entry point for the FlatAttention software implementation.

## The `FlatAttention` Class (from `attn.py`) ✨

```python
class FlatAttention:
    def __init__(self):
        # Attention parameters
        self.sequence_length         = 512
        self.head_dimension          = 128
        self.num_head                = 32
        self.batch_size              = 1
        self.elem_size               = 2

        # Flatten Settings
        # [Scale]: Number of clusters (x=scale, y=scale) assigned to one head. A set of clusters is referred to as a "Group."
        self.flatten_scale           = 4

        # [Shape]: The attention matrix shape (x=shape, y=shape) that each Group handles in each iteration.
        self.flatten_shape           = 512

        # [Layout]: Data layout scheme. Possible values:
        # - "tiled      QOKV": QO in west HBM, KV in south HBM
        # - "tiled      QKOV": QK in west HBM, OV in south HBM
        # - "tiled      QVOK": QV in west HBM, OK in south HBM
        # - "tiled      OKQV": OK in west HBM, QV in south HBM
        # - "tiled      OVQK": OV in west HBM, QK in south HBM
        # - "tiled      KVQO": KV in west HBM, QO in south HBM
        self.flatten_layout_method   = "QOKV"

        # [Async]: Whether to enable asynchronous execution
        self.flatten_async           = 1

        # [Numerical]: Whether to check numerical correctness
        self.flatten_numerical_check = 0
```

## Usage 🚀

1. **Initialize the SoftHier Platform**  
   Navigate to the root directory of `FlatAttention` and run:
   ```bash
   source sourceme.sh
   ```
   This command will download and initialize the SoftHier platform in the `build` folder.

2. **Configure `attn.py` and `arch.py`**  
   Modify these files (in `configs/`) to meet your specific requirements before running the simulation.

3. **Build and Run the Simulation**  
   ```bash
   make run
   ```
   - If `self.flatten_numerical_check = 1` in `attn.py`, the workflow will:
     1. Generate HBM preload data and golden results in `build/preload`.
     2. Run the simulation with the preload data.
     3. Compare the dumped simulation results with the golden results for verification.
   - If `self.flatten_numerical_check = 0`, the simulation runs faster, focusing only on performance without numerical checks.

   ***NOTE:*** the spatz will generate the whole RVV instruction trace, which is a big burden for visulization when simulating FlatAttetnion on scalable SoftHier configuration. For such a case, here we highly recommand to use
   ```bash
   make run_simple_trace
   ```

4. **Generate and View the Perfetto Trace**  
   After the simulation completes, you can generate a Perfetto trace by running:
   ```bash
   make pfto
   ```
   The resulting trace is located in `build/perfetto`. To view it, open 👉 [Perfetto UI](https://ui.perfetto.dev/) in your browser and load the generated trace file.
