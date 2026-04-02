#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$SCRIPT_DIR/Il2CppDumperGUI/Il2CppDumperGUI/Il2CppPostProcess"
OUT_DIR="$SCRIPT_DIR/Il2CppDumperGUI/Il2CppDumperGUI/Resources"

# Find dotnet
if [ -x "/opt/homebrew/opt/dotnet@8/libexec/dotnet" ]; then
    DOTNET="/opt/homebrew/opt/dotnet@8/libexec/dotnet"
    export DOTNET_ROOT="/opt/homebrew/opt/dotnet@8/libexec"
elif [ -x "/usr/local/share/dotnet/dotnet" ]; then
    DOTNET="/usr/local/share/dotnet/dotnet"
    export DOTNET_ROOT="/usr/local/share/dotnet"
else
    echo "ERROR: dotnet not found. Install: brew install dotnet@8"
    exit 1
fi

echo "Using: $DOTNET"
"$DOTNET" --version
echo ""
cd "$PROJ_DIR"

echo "=== Building arm64 (Apple Silicon) ==="
"$DOTNET" publish Il2CppPostProcess.csproj \
    -r osx-arm64 --self-contained -c Release \
    -p:PublishSingleFile=true -p:PublishTrimmed=true \
    -p:InvariantGlobalization=true \
    -o bin/publish-arm64

echo ""
echo "=== Building x64 (Intel) ==="
"$DOTNET" publish Il2CppPostProcess.csproj \
    -r osx-x64 --self-contained -c Release \
    -p:PublishSingleFile=true -p:PublishTrimmed=true \
    -p:InvariantGlobalization=true \
    -o bin/publish-x64

echo ""
echo "=== Creating universal binary ==="
mkdir -p "$OUT_DIR"
lipo -create \
    bin/publish-arm64/Il2CppPostProcess \
    bin/publish-x64/Il2CppPostProcess \
    -output "$OUT_DIR/Il2CppPostProcess"
chmod +x "$OUT_DIR/Il2CppPostProcess"
cp "$PROJ_DIR/Libraries/Il2CppDummyDll.dll" "$OUT_DIR/Il2CppDummyDll.dll"

echo ""
echo "=== SUCCESS ==="
file "$OUT_DIR/Il2CppPostProcess"
ls -lh "$OUT_DIR/Il2CppPostProcess" "$OUT_DIR/Il2CppDummyDll.dll"
echo ""
echo "Now rebuild the Xcode project."
