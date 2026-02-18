#!/usr/bin/env bash
# Test anolis-provider-sim (Linux/macOS)
#
# Usage:
#   ./scripts/test.sh [options]
#
# Options:
#   --suite <name>         Test suite: all|smoke|adpp|multi|fault (default: all)
#   --test <name>          Alias for --suite
#   --build-dir <path>     Build directory (default: build)
#   --tsan                 Use build-tsan as build directory
#   --exe <path>           Path to anolis-provider-sim executable
#   --python <command>     Python command (default: python3, fallback: python)
#   --no-generate          Skip protobuf Python binding generation
#   -h, --help             Show this help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

SUITE="all"
BUILD_DIR="$REPO_ROOT/build"
EXE_PATH=""
PYTHON_CMD="${PYTHON_CMD:-python3}"
GENERATE=true

usage() {
    sed -n '1,40p' "$0"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    --suite | --test)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] $1 requires a value"
            exit 2
        }
        SUITE="$2"
        shift 2
        ;;
    --build-dir)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] --build-dir requires a value"
            exit 2
        }
        BUILD_DIR="$2"
        shift 2
        ;;
    --tsan)
        BUILD_DIR="$REPO_ROOT/build-tsan"
        shift
        ;;
    --exe)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] --exe requires a value"
            exit 2
        }
        EXE_PATH="$2"
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

if ! command -v "$PYTHON_CMD" >/dev/null 2>&1; then
    if command -v python >/dev/null 2>&1; then
        PYTHON_CMD="python"
    else
        echo "[ERROR] Python not found. Install python3 or set --python."
        exit 1
    fi
fi

if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
fi
export ANOLIS_PROVIDER_SIM_BUILD_DIR="$BUILD_DIR"

if [[ "$GENERATE" == true ]]; then
    "$SCRIPT_DIR/generate-proto-python.sh"
fi

# Validate Python protobuf runtime for the selected interpreter.
if ! "$PYTHON_CMD" -c "import google.protobuf" >/dev/null 2>&1; then
    echo "[ERROR] Python package 'protobuf' is missing for: $PYTHON_CMD"
    echo "Install it with:"
    echo "  $PYTHON_CMD -m pip install protobuf"
    exit 1
fi

resolve_exe() {
    if [[ -n "$EXE_PATH" ]]; then
        if [[ -f "$EXE_PATH" ]]; then
            printf "%s\n" "$EXE_PATH"
            return
        fi
        echo "[ERROR] --exe path not found: $EXE_PATH"
        exit 1
    fi

    local candidates=(
        "$BUILD_DIR/anolis-provider-sim"
        "$BUILD_DIR/Release/anolis-provider-sim"
        "$BUILD_DIR/Debug/anolis-provider-sim"
        "$BUILD_DIR/anolis-provider-sim.exe"
        "$BUILD_DIR/Release/anolis-provider-sim.exe"
        "$BUILD_DIR/Debug/anolis-provider-sim.exe"
        "$REPO_ROOT/build-tsan/anolis-provider-sim"
    )
    local candidate
    for candidate in "${candidates[@]}"; do
        if [[ -f "$candidate" ]]; then
            printf "%s\n" "$candidate"
            return
        fi
    done
    echo "[ERROR] Could not find anolis-provider-sim executable."
    echo "Build first with: ./scripts/build.sh"
    exit 1
}

run_case() {
    local name="$1"
    shift
    echo "[INFO] Running $name..."
    "$PYTHON_CMD" "$@"
}

case "$SUITE" in
all | smoke | adpp | multi | fault) ;;
*)
    echo "[ERROR] Invalid suite: $SUITE"
    echo "Valid values: all, smoke, adpp, multi, fault"
    exit 2
    ;;
esac

cd "$REPO_ROOT"
EXE_PATH="$(resolve_exe)"
export ANOLIS_PROVIDER_SIM_EXE="$EXE_PATH"

echo "[INFO] Using executable: $ANOLIS_PROVIDER_SIM_EXE"

if [[ "$SUITE" == "all" || "$SUITE" == "smoke" ]]; then
    run_case "smoke test" tests/test_hello.py
fi

if [[ "$SUITE" == "all" || "$SUITE" == "adpp" ]]; then
    run_case "ADPP integration tests" tests/test_adpp_integration.py --test all
fi

if [[ "$SUITE" == "all" || "$SUITE" == "multi" ]]; then
    run_case "multi-instance test" tests/test_multi_instance.py --config config/multi-tempctl.yaml
fi

if [[ "$SUITE" == "all" || "$SUITE" == "fault" ]]; then
    run_case "fault injection tests" tests/test_fault_injection.py --test all
fi

echo "[INFO] Test suite complete"
