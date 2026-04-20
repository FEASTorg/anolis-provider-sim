#!/usr/bin/env bash
# Generate Python protobuf bindings for test scripts

set -e

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -z "${ANOLIS_PROTOCOL_PROTO_DIR:-}" ]]; then
    echo "ERROR: ANOLIS_PROTOCOL_PROTO_DIR is not set"
    exit 1
fi
proto_root="$ANOLIS_PROTOCOL_PROTO_DIR"
proto_v1_dir="$proto_root/anolis/deviceprovider/v1"
output_dir="${1:-${ANOLIS_PROVIDER_SIM_BUILD_DIR:-$repo_root/build}}"
if [[ "$output_dir" != /* ]]; then
    output_dir="$repo_root/$output_dir"
fi

echo "Generating Python protobuf bindings..."

# Find protoc - check PATH first, then vcpkg installation
protoc_cmd="protoc"
if ! command -v protoc &> /dev/null; then
    # Check vcpkg installed location under selected output dir, then default build dir.
    vcpkg_candidates=(
        "$output_dir/vcpkg_installed/x64-linux/tools/protobuf/protoc"
        "$repo_root/build/vcpkg_installed/x64-linux/tools/protobuf/protoc"
    )
    for candidate in "${vcpkg_candidates[@]}"; do
        if [ -f "$candidate" ]; then
            protoc_cmd="$candidate"
            echo "  Using vcpkg protoc: $protoc_cmd"
            break
        fi
    done
    if [[ "$protoc_cmd" == "protoc" ]]; then
        echo "ERROR: protoc not found in PATH or vcpkg installation"
        echo "Install Protocol Buffers compiler from: https://github.com/protocolbuffers/protobuf/releases"
        exit 1
    fi
fi

# Check if split proto directory exists
if [ ! -d "$proto_v1_dir" ]; then
    echo "ERROR: Protocol directory not found: $proto_v1_dir"
    exit 1
fi

mapfile -t proto_files < <(find "$proto_v1_dir" -maxdepth 1 -name '*.proto' -printf '%f\n' | sort)
if [ ${#proto_files[@]} -eq 0 ]; then
    echo "ERROR: No .proto files found in: $proto_v1_dir"
    exit 1
fi

proto_args=()
for file in "${proto_files[@]}"; do
    proto_args+=("anolis/deviceprovider/v1/$file")
done

# Create output directory if needed
mkdir -p "$output_dir"

# Generate Python bindings
echo "  Proto dir: $proto_v1_dir"
echo "  Output dir: $output_dir"

$protoc_cmd --python_out="$output_dir" --proto_path="$proto_root" "${proto_args[@]}"

compat_file="$output_dir/protocol_pb2.py"
cat > "$compat_file" <<'EOF'
"""Compatibility shim for legacy protocol_pb2 imports.

Generated from split ADPP v1 protos in anolis/deviceprovider/v1.
"""

from anolis.deviceprovider.v1.call_pb2 import *
from anolis.deviceprovider.v1.envelope_pb2 import *
from anolis.deviceprovider.v1.handshake_pb2 import *
from anolis.deviceprovider.v1.health_pb2 import *
from anolis.deviceprovider.v1.inventory_pb2 import *
from anolis.deviceprovider.v1.readiness_pb2 import *
from anolis.deviceprovider.v1.status_pb2 import *
from anolis.deviceprovider.v1.telemetry_pb2 import *
from anolis.deviceprovider.v1.types_pb2 import *
from anolis.deviceprovider.v1.value_pb2 import *
EOF

echo "[PASS] Generated Python protos under: $output_dir/anolis/deviceprovider/v1"
echo "[PASS] Generated compatibility shim: $compat_file"
