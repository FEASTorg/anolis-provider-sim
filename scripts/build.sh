#!/usr/bin/env bash
# Build anolis-provider-sim (Linux/macOS)
#
# Usage:
#   ./scripts/build.sh [options]
#
# Options:
#   --clean                 Remove build directory before configure
#   --debug                 Build Debug (default: Release)
#   --release               Build Release
#   --tsan                  Enable ThreadSanitizer (Linux only)
#   --build-dir <path>      Override build directory
#   --generator <name>      CMake generator (default: Ninja if available)
#   -j, --jobs <N>          Parallel build jobs
#   -h, --help              Show this help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

BUILD_TYPE="Release"
BUILD_DIR=""
CLEAN=false
TSAN=false
GENERATOR=""
JOBS=""

usage() {
    sed -n '1,40p' "$0"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    --clean)
        CLEAN=true
        shift
        ;;
    --debug)
        BUILD_TYPE="Debug"
        shift
        ;;
    --release)
        BUILD_TYPE="Release"
        shift
        ;;
    --tsan)
        TSAN=true
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
    --generator)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] --generator requires a value"
            exit 2
        }
        GENERATOR="$2"
        shift 2
        ;;
    -j | --jobs)
        [[ $# -lt 2 ]] && {
            echo "[ERROR] $1 requires a value"
            exit 2
        }
        JOBS="$2"
        shift 2
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

if [[ -z "$BUILD_DIR" ]]; then
    BUILD_DIR="$REPO_ROOT/build"
    if [[ "$TSAN" == true ]]; then
        BUILD_DIR="$REPO_ROOT/build-tsan"
    fi
fi

if [[ "$TSAN" == true ]]; then
    if [[ "$(uname -s)" != "Linux" ]]; then
        echo "[ERROR] --tsan is supported only on Linux for this project setup."
        exit 1
    fi
fi

if [[ -z "${VCPKG_ROOT:-}" ]]; then
    for candidate in "$HOME/tools/vcpkg" "$HOME/vcpkg" "/opt/vcpkg"; do
        if [[ -d "$candidate/scripts/buildsystems" ]]; then
            export VCPKG_ROOT="$candidate"
            break
        fi
    done
fi

if [[ -z "${VCPKG_ROOT:-}" || ! -d "$VCPKG_ROOT/scripts/buildsystems" ]]; then
    echo "[ERROR] VCPKG_ROOT is not set or invalid."
    echo "Set VCPKG_ROOT to your vcpkg installation path."
    exit 1
fi

TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
TRIPLET="x64-linux"
if [[ "$(uname -s)" == "Darwin" ]]; then
    TRIPLET="x64-osx"
fi
if [[ "$TSAN" == true ]]; then
    TRIPLET="x64-linux-tsan"
fi

if [[ -z "$GENERATOR" && $(command -v ninja >/dev/null 2>&1; echo $?) -eq 0 ]]; then
    GENERATOR="Ninja"
fi

GENERATOR_ARGS=()
if [[ -n "$GENERATOR" ]]; then
    GENERATOR_ARGS=(-G "$GENERATOR")
fi

if [[ "$CLEAN" == true ]]; then
    echo "[INFO] Cleaning build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

CONFIG_ARGS=(
    -S "$REPO_ROOT"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
    -DVCPKG_TARGET_TRIPLET="$TRIPLET"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

if [[ "$TSAN" == true ]]; then
    CONFIG_ARGS+=(-DENABLE_TSAN=ON)
fi

BUILD_ARGS=(--build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel)
if [[ -n "$JOBS" ]]; then
    BUILD_ARGS=(--build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$JOBS")
fi

echo "[INFO] Repo root: $REPO_ROOT"
echo "[INFO] Build dir: $BUILD_DIR"
echo "[INFO] Build type: $BUILD_TYPE"
echo "[INFO] Triplet: $TRIPLET"
echo "[INFO] TSAN: $TSAN"

cmake "${GENERATOR_ARGS[@]}" "${CONFIG_ARGS[@]}"
cmake "${BUILD_ARGS[@]}"

echo "[INFO] Build complete"
