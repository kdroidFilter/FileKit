#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESOURCES_DIR="$SCRIPT_DIR/../../resources/filekit/native"

# Detect JDK
JAVA_HOME="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(which javac)")")")}"
JNI_INCLUDE="$JAVA_HOME/include"
JNI_INCLUDE_LINUX="$JNI_INCLUDE/linux"

SRC="$SCRIPT_DIR/filekit_linux_window.c"

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    OUT_DIR="$RESOURCES_DIR/linux-aarch64"
else
    OUT_DIR="$RESOURCES_DIR/linux-x64"
fi

mkdir -p "$OUT_DIR"

gcc -o "$OUT_DIR/libfilekit_linux_window.so" "$SRC" \
    -shared -fPIC \
    -I"$JNI_INCLUDE" -I"$JNI_INCLUDE_LINUX" \
    -L"$JAVA_HOME/lib" -ljawt \
    -Os -flto -fvisibility=hidden \
    -Wl,--gc-sections -ffunction-sections -fdata-sections \
    -s

echo "Built: $OUT_DIR/libfilekit_linux_window.so"
