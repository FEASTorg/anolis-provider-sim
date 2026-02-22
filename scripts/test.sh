#!/usr/bin/env bash
# Test anolis-provider-sim via CTest presets.
#
# Usage:
#   ./scripts/test.sh [options] [-- <extra-ctest-args>]
#
# Options:
#   --preset <name>   Test preset (default: dev-release)
#   --suite <name>    all|smoke|adpp|multi|fault|fluxgraph (default: all)
#   -v, --verbose     Run ctest with -VV
#   -h, --help        Show help

set -euo pipefail

PRESET="dev-release"
SUITE="all"
VERBOSE=false
EXTRA_CTEST_ARGS=()

usage() {
    sed -n '1,18p' "$0"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    --preset)
        [[ $# -lt 2 ]] && { echo "[ERROR] --preset requires a value"; exit 2; }
        PRESET="$2"
        shift 2
        ;;
    --suite | --test)
        [[ $# -lt 2 ]] && { echo "[ERROR] $1 requires a value"; exit 2; }
        SUITE="$2"
        shift 2
        ;;
    --suite=* | --test=*)
        SUITE="${1#*=}"
        shift
        ;;
    -v | --verbose)
        VERBOSE=true
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    --)
        shift
        EXTRA_CTEST_ARGS=("$@")
        break
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

CTEST_ARGS=(--preset "$PRESET")
if [[ "$SUITE" == "all" ]]; then
    CTEST_ARGS+=(-L "provider")
else
    CTEST_ARGS+=(-L "$SUITE")
fi
if [[ "$VERBOSE" == true ]]; then
    CTEST_ARGS+=(-VV)
fi
if [[ ${#EXTRA_CTEST_ARGS[@]} -gt 0 ]]; then
    CTEST_ARGS+=("${EXTRA_CTEST_ARGS[@]}")
fi

echo "[INFO] Test preset: $PRESET"
echo "[INFO] Suite: $SUITE"
ctest "${CTEST_ARGS[@]}"
