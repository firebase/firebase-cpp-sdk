#!/bin/bash
# Orchestrates a complete, universal full-platform release compilation in a single command!

set -e

echo "========================================================"
echo "   Executing Universal Full-Platform Release Build      "
echo "========================================================"

# 1. Apple Platforms
echo "Building iOS Device (arm64)..."
./scripts/gha/build.py --preset ios-arm64

echo "Building iOS Simulator (Universal)..."
./scripts/gha/build.py --preset ios-sim-universal

echo "Building macOS Apple Silicon (arm64)..."
./scripts/gha/build.py --preset macos-arm64

# 2. Android NDK
echo "Building Android (arm64-v8a)..."
./scripts/gha/build.py --preset android-arm64

# 3. Desktop
echo "Building Linux (x86_64)..."
./scripts/gha/build.py --preset linux-x64

echo "========================================================"
echo "   All platforms compiled and assembled successfully!   "
echo "========================================================"
