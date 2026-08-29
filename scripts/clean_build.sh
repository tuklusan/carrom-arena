#!/bin/bash
# Clean Build Script - Carrom Arena
# Removes build directory and rebuilds from scratch

set -e

echo "=== Carrom Arena Clean Build ==="
echo "Removing build directory..."
rm -rf build

echo "Creating build directory..."
mkdir -p build

echo "Configuring CMake..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja

echo "Building..."
cmake --build build --parallel $(nproc)

echo "=== Clean Build Complete ==="
echo "Binary: build/carrom_arena"
