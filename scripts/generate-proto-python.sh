#!/usr/bin/env bash
# Generate Python protobuf bindings for test scripts

set -e

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
proto_file="$repo_root/external/anolis/spec/device-provider/protocol.proto"
proto_path="$repo_root/external/anolis/spec/device-provider"
output_dir="${ANOLIS_PROVIDER_SIM_BUILD_DIR:-$repo_root/build}"
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

# Check if proto file exists
if [ ! -f "$proto_file" ]; then
    echo "ERROR: Protocol file not found: $proto_file"
    exit 1
fi

# Create output directory if needed
mkdir -p "$output_dir"

# Generate Python bindings
echo "  Proto file: $proto_file"
echo "  Output dir: $output_dir"

$protoc_cmd --python_out="$output_dir" --proto_path="$proto_path" protocol.proto

output_file="$output_dir/protocol_pb2.py"
echo "✓ Generated: $output_file"
