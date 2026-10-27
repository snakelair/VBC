#!/usr/bin/env bash
set -e

echo "==================================================="
echo "VBC Native Compiler Build Script (Pure C99 Bootstrap)"
echo "==================================================="

if [ "$1" = "clean" ]; then
    echo "Cleaning build artifacts..."
    rm -f vbc temp_out* temp_test*
    echo "Done."
    exit 0
fi

if [ "$1" = "rebuild" ]; then
    if [ ! -f ./vbc ]; then
        echo "Error: ./vbc not found. Run ./build.sh first."
        exit 1
    fi
    echo "Performing self-hosted recompilation of src/ ..."
    ./vbc compile src -o vbc_new --emit-c
    mv -f vbc_new vbc
    if [ -f vbc_new.c ]; then
        cp -f vbc_new.c bootstrap/vbc_bootstrap.c
        rm -f vbc_new.c
    fi
    echo "Self-hosted rebuild complete: ./vbc and bootstrap/vbc_bootstrap.c updated!"
    exit 0
fi

CC=""
if command -v clang >/dev/null 2>&1; then
    CC="clang"
elif command -v gcc >/dev/null 2>&1; then
    CC="gcc"
else
    echo "Error: Neither clang nor gcc found in PATH. Please install Clang or GCC."
    exit 1
fi

echo "Building vbc with $CC from bootstrap/vbc_bootstrap.c..."
$CC -O2 -w bootstrap/vbc_bootstrap.c runtime/src/*.c -I runtime/include -lpthread -lm -o vbc
echo "Build successful: ./vbc"
