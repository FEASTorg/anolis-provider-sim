#!/usr/bin/env bash
# Test anolis-provider-sim suites against a preset build.
#
# Usage:
#   ./scripts/test.sh [options]
#
# Options:
#   --preset <name>   Build preset to test (default: dev-release)
#   --suite <name>    all|smoke|adpp|multi|fault|fluxgraph (default: all)
#   --python <cmd>    Python interpreter (default: python3, fallback: python)
#   --no-generate     Skip protobuf Python binding generation
#   -h, --help        Show help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

PRESET="dev-release"
SUITE="all"
PYTHON_CMD="${PYTHON_CMD:-python3}"
GENERATE=true

usage() {
    sed -n '1,24p' "$0"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    --preset)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] --preset requires a value"
            exit 2
        }
        PRESET="$2"
        shift 2
        ;;
    --suite | --test)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] $1 requires a value"
            exit 2
        }
        SUITE="$2"
        shift 2
        ;;
    --python)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] --python requires a value"
            exit 2
        }
        PYTHON_CMD="$2"
        shift 2
        ;;
    --no-generate)
        GENERATE=false
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        echo "[ERROR] Unknown option: $1"
        usage
        exit 2
        ;;
    esac
done

case "$SUITE" in
all | smoke | adpp | multi | fault | fluxgraph) ;;
*)
    echo "[ERROR] Invalid suite: $SUITE"
    echo "Valid values: all, smoke, adpp, multi, fault, fluxgraph"
    exit 2
    ;;
esac

if ! command -v "$PYTHON_CMD" >/dev/null 2>&1; then
    if command -v python >/dev/null 2>&1; then
        PYTHON_CMD="python"
    else
        echo "[ERROR] Python not found. Install python3 or pass --python."
        exit 1
    fi
fi

BUILD_DIR="$REPO_ROOT/build/$PRESET"
export ANOLIS_PROVIDER_SIM_BUILD_DIR="$BUILD_DIR"

if [[ "$GENERATE" == true ]]; then
    "$SCRIPT_DIR/generate_proto_python.sh" "$BUILD_DIR"
fi

if ! "$PYTHON_CMD" -c "import google.protobuf" >/dev/null 2>&1; then
    echo "[ERROR] Python package 'protobuf' is missing for: $PYTHON_CMD"
    echo "Install it with:"
    echo "  $PYTHON_CMD -m pip install protobuf"
    exit 1
fi

resolve_exe() {
    local candidates=(
        "$BUILD_DIR/anolis-provider-sim"
        "$BUILD_DIR/anolis-provider-sim.exe"
        "$BUILD_DIR/Release/anolis-provider-sim"
        "$BUILD_DIR/Release/anolis-provider-sim.exe"
        "$BUILD_DIR/Debug/anolis-provider-sim"
        "$BUILD_DIR/Debug/anolis-provider-sim.exe"
    )
    local candidate
    for candidate in "${candidates[@]}"; do
        if [[ -f "$candidate" ]]; then
            printf "%s\n" "$candidate"
            return
        fi
    done
    echo "[ERROR] Could not find anolis-provider-sim executable in: $BUILD_DIR"
    echo "Build first with: ./scripts/build.sh --preset $PRESET"
    exit 1
}

require_fluxgraph_enabled() {
    local cmake_cache="$BUILD_DIR/CMakeCache.txt"
    if [[ ! -f "$cmake_cache" ]]; then
        echo "[ERROR] FluxGraph suite requires configured build: $BUILD_DIR"
        exit 1
    fi
    if ! grep -Eq '^ENABLE_FLUXGRAPH:BOOL=ON$' "$cmake_cache"; then
        echo "[ERROR] FluxGraph suite requested but preset '$PRESET' has ENABLE_FLUXGRAPH=OFF."
        echo "Rebuild with: ./scripts/build.sh --preset ci-linux-release-fluxgraph"
        exit 1
    fi
}

run_case() {
    local name="$1"
    shift
    echo "[INFO] Running $name..."
    "$PYTHON_CMD" "$@"
}

cd "$REPO_ROOT"
export ANOLIS_PROVIDER_SIM_EXE="$(resolve_exe)"
echo "[INFO] Using executable: $ANOLIS_PROVIDER_SIM_EXE"
echo "[INFO] Using preset: $PRESET"

if [[ "$SUITE" == "fluxgraph" ]]; then
    require_fluxgraph_enabled
fi

if [[ "$SUITE" == "all" || "$SUITE" == "smoke" ]]; then
    run_case "smoke test" tests/test_hello.py
fi
if [[ "$SUITE" == "all" || "$SUITE" == "adpp" ]]; then
    run_case "ADPP integration tests" tests/test_adpp_integration.py --test all
fi
if [[ "$SUITE" == "all" || "$SUITE" == "multi" ]]; then
    run_case "multi-instance test" tests/test_multi_instance.py --config config/multi-tempctl.yaml
fi
if [[ "$SUITE" == "fluxgraph" ]]; then
    run_case "FluxGraph integration test" tests/test_fluxgraph_integration.py
    run_case "multi-provider scenario" tests/test_multi_provider_scenario.py
fi
if [[ "$SUITE" == "all" || "$SUITE" == "fault" ]]; then
    run_case "fault injection tests" tests/test_fault_injection.py --test all
fi

echo "[INFO] Test suite complete"
