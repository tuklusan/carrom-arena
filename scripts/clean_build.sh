#!/bin/bash
# Clean Build Script - Carrom Arena
# Removes the out/ build directory and rebuilds from scratch

set -e

echo "=== Carrom Arena Clean Build ==="
echo "Removing out directory..."
rm -rf out

echo "Creating out directory..."
mkdir -p out

echo "Configuring CMake..."
cmake -B out -DCMAKE_BUILD_TYPE=Debug -G Ninja

echo "Building..."
cmake --build out --parallel $(nproc)

echo "=== Clean Build Complete ==="
echo "Binary: out/carrom_arena"
