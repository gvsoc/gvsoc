# Example Configuration for Reproducing Results from the *FlatAttention* Paper

## Usage

Please navigate to the root directory of the **FlatAttention** repository:

```bash
cd ..
```

Choose the specific experiment you would like to reproduce (e.g., `Fig3_D128_S4096_FlatAsync`). To run the simulation, use the following command:

```bash
arch=paper_examples/Fig3_D128_S4096_FlatAsync/arch.py attn=paper_examples/Fig3_D128_S4096_FlatAsync/attn.py make run_simple_trace
```

After the simulation completes, you can generate a Perfetto trace with:

```bash
make pfto
```

## We are continue updating ...
