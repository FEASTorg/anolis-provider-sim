#!/usr/bin/env bash
# Anolis Provider Simulator - Local Run Script (Linux/macOS)
#
# Usage:
#   ./scripts/run_local.sh [--preset <name>] [--build-dir DIR] [-- <provider-args>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

PRESET="dev-release"
BUILD_DIR=""
PROVIDER_ARGS=()

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
    --preset=*)
        PRESET="${1#*=}"
        shift
        ;;
    --build-dir)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] --build-dir requires a value"
            exit 2
        }
        BUILD_DIR="$2"
        shift 2
        ;;
    --build-dir=*)
        BUILD_DIR="${1#*=}"
        shift
        ;;
    --)
        shift
        PROVIDER_ARGS=("$@")
        break
        ;;
    *)
        echo "[ERROR] Unknown argument: $1"
        exit 2
        ;;
    esac
done

if [[ -z "$BUILD_DIR" ]]; then
    BUILD_DIR="$REPO_ROOT/build/$PRESET"
fi

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "[ERROR] Build directory not found: $BUILD_DIR"
    echo "Run: bash ./scripts/build.sh --preset $PRESET"
    exit 1
fi

PROVIDER=""
for CANDIDATE in \
    "$BUILD_DIR/anolis-provider-sim" \
    "$BUILD_DIR/Release/anolis-provider-sim" \
    "$BUILD_DIR/Debug/anolis-provider-sim"; do
    if [[ -x "$CANDIDATE" ]]; then
        PROVIDER="$CANDIDATE"
        break
    fi
done

if [[ -z "$PROVIDER" ]]; then
    echo "[ERROR] Provider executable not found under: $BUILD_DIR"
    echo "Run: bash ./scripts/build.sh --preset $PRESET"
    exit 1
fi

if grep -q "ENABLE_TSAN:BOOL=ON" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
    echo "[INFO] ThreadSanitizer build detected"

    VCPKG_TRIPLET=$(grep "VCPKG_TARGET_TRIPLET:STRING=" "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2 || true)
    if [[ -z "${VCPKG_TRIPLET:-}" ]]; then
        echo "[WARN] Could not determine VCPKG_TARGET_TRIPLET, defaulting to x64-linux"
        VCPKG_TRIPLET="x64-linux"
    fi

    export LD_LIBRARY_PATH="$BUILD_DIR/vcpkg_installed/$VCPKG_TRIPLET/lib:${LD_LIBRARY_PATH:-}"
    export TSAN_OPTIONS="second_deadlock_stack=1 detect_deadlocks=1 history_size=7"

    echo "[INFO] Using VCPKG triplet: $VCPKG_TRIPLET"
fi

echo "[INFO] Starting Anolis Provider Simulator..."
echo "[INFO] Preset:          $PRESET"
echo "[INFO] Build directory: $BUILD_DIR"
echo "[INFO] Executable:      $PROVIDER"
echo ""
echo "[INFO] Provider communicates via stdin/stdout using protobuf framing."
echo "[INFO] Press Ctrl+C to stop."
echo ""

exec "$PROVIDER" "${PROVIDER_ARGS[@]}"
