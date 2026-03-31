#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RESOURCES_DIR="$SCRIPT_DIR/../../resources/filekit/native"

# Detect JDK
JAVA_HOME="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(which javac)")")")}"
JNI_INCLUDE="$JAVA_HOME/include"
JNI_INCLUDE_LINUX="$JNI_INCLUDE/linux"

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    OUT_DIR="$RESOURCES_DIR/linux-aarch64"
else
    OUT_DIR="$RESOURCES_DIR/linux-x64"
fi

mkdir -p "$OUT_DIR"

COMMON_FLAGS="-shared -fPIC -I$JNI_INCLUDE -I$JNI_INCLUDE_LINUX -Os -flto -fvisibility=hidden -Wl,--gc-sections -ffunction-sections -fdata-sections -s"

# 1. Build JAWT window ID library
gcc -o "$OUT_DIR/libfilekit_linux_window.so" \
    "$SCRIPT_DIR/filekit_linux_window.c" \
    $COMMON_FLAGS \
    -L"$JAVA_HOME/lib" -ljawt

echo "Built: $OUT_DIR/libfilekit_linux_window.so"

# 2. Build XDG portal file chooser library (requires libdbus-1)
DBUS_CFLAGS=$(pkg-config --cflags dbus-1)
DBUS_LIBS=$(pkg-config --libs dbus-1)

gcc -o "$OUT_DIR/libfilekit_xdg_portal.so" \
    "$SCRIPT_DIR/filekit_xdg_portal.c" \
    $COMMON_FLAGS \
    $DBUS_CFLAGS \
    -lpthread \
    $DBUS_LIBS

echo "Built: $OUT_DIR/libfilekit_xdg_portal.so"
