#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/.build/native-engine"

cmake -S "$SCRIPT_DIR/NativeEngine" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'
cmake --build "$BUILD_DIR" --config Release --parallel
file "$BUILD_DIR/libIl2CppDumperNative.a"
