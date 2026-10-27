#!/bin/sh
set -e

echo "Building VBC Pure Native Self-Hosted Compiler..."

CC="${CC:-clang}"
if ! command -v "$CC" >/dev/null 2>&1; then
    CC="gcc"
fi

if ! command -v "$CC" >/dev/null 2>&1; then
    echo "Error: Neither clang nor gcc found in PATH. Please install clang or gcc."
    exit 1
fi

mkdir -p bin

OS_NAME="$(uname -s)"
LDFLAGS="-lpthread -lm"
if [ "$OS_NAME" = "Darwin" ]; then
    LDFLAGS="-lpthread"
fi

echo "Using compiler: $CC ($OS_NAME)"
$CC -O3 -std=c11 -I runtime/include \
    bin/vbc-stage3.exe.c \
    runtime/src/vbc_socket.c \
    runtime/src/vbc_http.c \
    $LDFLAGS \
    -o bin/vbc

echo "Build successful! Binary location: bin/vbc"
echo "Test run:"
./bin/vbc
