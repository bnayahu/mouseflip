#!/bin/bash
# Copyright 2026 Jonathan Bnayahu
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

echo "Building Primary with MinGW-w64..."

# MinGW-w64 cross-compiler tools
WINDRES="x86_64-w64-mingw32-windres"
GCC="x86_64-w64-mingw32-g++"

# Check both tools, not just the compiler
for tool in "$GCC" "$WINDRES"; do
    if ! command -v "$tool" &> /dev/null; then
        echo "Error: $tool not found!"
        echo "Install it with: sudo apt install mingw-w64"
        exit 1
    fi
done

# Compile resources
echo "Compiling resources..."
"$WINDRES" resources/primary.rc -O coff -o resources/primary.res

# Compile and link
#   -municode      : entry point is wWinMain; omitting this fails to link
#   -Os -Wl,-s     : optimise for size and strip symbols (~376K -> ~114K)
#   -mwindows      : GUI subsystem, no console window
echo "Compiling application..."
"$GCC" -std=c++11 -Wall -Wextra -Wno-unused-parameter \
     -Os -DUNICODE -D_UNICODE \
     -mwindows -municode \
     src/primary.cpp \
     resources/primary.res \
     -o Primary.exe \
     -luser32 -lshell32 -lcomctl32 -static-libgcc -static-libstdc++ \
     -Wl,-s

echo "Build successful! Output: Primary.exe ($(stat -c%s Primary.exe) bytes)"
