#!/bin/bash
# Build script for sral_bridge.dll (cross-compile from Linux to Windows x64)
#
# Prerequisites:
#   sudo apt install gcc-mingw-w64-x86-64 liblua5.1-0-dev
#
# This script:
#   1. Creates the lua51.dll import library from the .def file
#   2. Compiles sral_bridge.dll (Lua C module for Mudlet)
#   3. Packages everything into sral-mudlet.mpackage

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/dist"

mkdir -p "$BUILD_DIR" "$DIST_DIR"

echo "=== Creating lua51.dll import library ==="
x86_64-w64-mingw32-dlltool -d "$SRC_DIR/lua51.def" -l "$BUILD_DIR/liblua51.a"

echo "=== Compiling sral_bridge.dll ==="
x86_64-w64-mingw32-gcc -shared \
    -o "$DIST_DIR/sral_bridge.dll" \
    "$SRC_DIR/lua_sral.c" \
    -I/usr/include/lua5.1 \
    -L"$BUILD_DIR" -llua51 \
    -O2 -Wall

echo "=== Building sral-mudlet.mpackage ==="
cd "$SCRIPT_DIR"
rm -f "$DIST_DIR/sral-mudlet.mpackage"
zip -j "$DIST_DIR/sral-mudlet.mpackage" \
    sral-mudlet.xml \
    "$DIST_DIR/sral_bridge.dll"

echo "=== Done ==="
echo "Output files in $DIST_DIR:"
ls -lh "$DIST_DIR"
