#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
DOTNET="${DOTNET:-$(command -v dotnet || true)}"
if [[ -z "$DOTNET" ]]; then
    echo 'Install the .NET 10 SDK or set DOTNET to its executable.' >&2
    exit 1
fi
PROJECT="$ROOT/vendor/AndnixSH/Il2CppDumper.CLI/Il2CppDumper.CLI.csproj"
RESOURCES="$ROOT/Il2CppDumperGUI/Resources"
for arch in arm64 x64; do
    "$DOTNET" publish "$PROJECT" -c Release -r "osx-$arch" --self-contained true \
        -p:PublishSingleFile=true -p:DebugType=none \
        -o "$ROOT/work/publish-$arch"
    install -m 755 "$ROOT/work/publish-$arch/Il2CppDumper" "$RESOURCES/Il2CppDumper-$arch"
done
printf 'Built both macOS CLI architectures in %s\n' "$RESOURCES"
