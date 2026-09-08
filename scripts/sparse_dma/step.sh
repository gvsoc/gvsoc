#!/usr/bin/env bash
set -eo pipefail
SPARSE_DMA_REPO="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
source "$SPARSE_DMA_REPO/build/reproduce_dramsys/env.sh"
cd "$SPARSE_DMA_REPO"
mkdir -p build/sparse_dma/logs build/sparse_dma/runs
export DRAMSYS_PATH="$SPARSE_DMA_REPO/build/sparse_dma/dram"
export SPARSE_DMA_CONFIG="${SPARSE_DMA_CONFIG:-$SPARSE_DMA_REPO/scripts/sparse_dma/config.json}"
export LD_LIBRARY_PATH="$SPARSE_DMA_REPO/build/sparse_dma/dramsys-rtl/build/lib:$LD_LIBRARY_PATH"
case "${1:-}" in
    prepare)
        python3 scripts/sparse_dma/prepare.py
        ;;
    build-dram)
        python3 scripts/sparse_dma/prepare_rtl_dramsys.py
        SPARSE_DMA_DRAM_SOURCE="$(cat build/sparse_dma/rtl-dramsys-source.txt)"
        cmake -S build/sparse_dma/dramsys-rtl -B build/sparse_dma/dramsys-rtl/build \
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DDRAMSYS_WITH_DRAMPOWER=OFF \
            -Dsystemc_DIR="$SYSTEMC_HOME/lib64/cmake/SystemCLanguage" \
            -DFETCHCONTENT_SOURCE_DIR_SQLITE3="$SPARSE_DMA_DRAM_SOURCE/build/_deps/sqlite3-src" \
            > build/sparse_dma/logs/dram-configure.log 2>&1
        cmake --build build/sparse_dma/dramsys-rtl/build -j 8 \
            > build/sparse_dma/logs/dram-build.log 2>&1
        ;;
    build-test)
        make TARGETS=sparse_dma_test CMAKE_FLAGS='-j 8' build > build/sparse_dma/logs/build-test.log 2>&1
        ;;
    test)
        timeout 120 ./install/bin/gvsoc --target-dir=install/targets --target=sparse_dma_test run > build/sparse_dma/logs/model-test.log 2>&1
        tail -n 12 build/sparse_dma/logs/model-test.log
        ;;
    build)
        make TARGETS='sparse_dma_test;spatz_sparse_dma' CMAKE_FLAGS='-j 8' build > build/sparse_dma/logs/build.log 2>&1
        ;;
    analyze)
        python3 scripts/sparse_dma/analyze.py
        ;;
    calibrate)
        python3 scripts/sparse_dma/calibrate.py "${@:2}"
        ;;
    build-validation)
        python3 scripts/sparse_dma/build_validation.py
        ;;
    profile)
        python3 scripts/sparse_dma/run_validation.py gvsoc --case "$2" --variant "${3:-gather-hw}" --trace-core
        ;;
    validate)
        case "${2:-}" in
            rtl)
                source "$SPARSE_DMA_REPO/../spatz-sparse-dma/build/reproduce_sparse_dma/env.sh"
                python3 "$SPARSE_DMA_REPO/scripts/sparse_dma/run_validation.py" rtl
                ;;
            gvsoc)
                python3 scripts/sparse_dma/run_validation.py gvsoc
                ;;
            *) echo 'validate requires rtl or gvsoc' >&2; exit 2 ;;
        esac
        ;;
    run)
        test -f "$SPARSE_DMA_REPO/build/sparse_dma/dramsys-rtl/build/lib/libDRAMSys_Simulator.so"
        SPARSE_DMA_VARIANT="${2:-gather-hw}"
        case "$SPARSE_DMA_VARIANT" in gather|gather-opt|gather-hw) ;; *) exit 2 ;; esac
        SPARSE_DMA_RUNDIR="$SPARSE_DMA_REPO/build/sparse_dma/runs/$SPARSE_DMA_VARIANT"
        mkdir -p "$SPARSE_DMA_RUNDIR"
        cd "$SPARSE_DMA_RUNDIR"
        cp "$SPARSE_DMA_CONFIG" config.json
        set +e
        timeout 300 "$SPARSE_DMA_REPO/install/bin/gvsoc" \
            --target-dir="$SPARSE_DMA_REPO/pulp/targets" --target=spatz_sparse_dma \
            --binary="$SPARSE_DMA_REPO/build/sparse_dma/binaries/$SPARSE_DMA_VARIANT.elf" \
            run --trace=idma --trace-level=trace > simulation.log 2>&1
        SPARSE_DMA_STATUS=$?
        set -e
        printf '%s\n' "$SPARSE_DMA_STATUS" > exit-status.txt
        tail -n 18 simulation.log
        test "$SPARSE_DMA_STATUS" -eq 0
        ;;
    *) echo "Usage: $0 {prepare|build-dram|build-test|test|build|analyze|calibrate|build-validation|validate rtl|validate gvsoc|profile CASE [VARIANT]|run [gather|gather-opt|gather-hw]}" >&2; exit 2 ;;
esac
