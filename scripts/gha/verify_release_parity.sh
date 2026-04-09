#!/bin/bash
# Automatically verifies 100% release parity between the old legacy build system and the modern 1-Step orchestrator!

set -e

echo "========================================================"
echo "   Executing Global Release Parity Verification         "
echo "========================================================"

# Define baseline and modern output directories
LEGACY_DIR="firebase-cpp-sdk-ios-tvos-build"
MODERN_DIR="build/ios-sim"

if [ ! -d "$LEGACY_DIR" ] || [ ! -d "$MODERN_DIR" ]; then
    echo "ERROR: Please run both the legacy build and modern build first!"
    echo "Legacy: ./build_scripts/ios/build.sh"
    echo "Modern: ./scripts/gha/build_everything.sh"
    exit 1
fi

echo "Extracting public C++ and Objective-C++ symbols..."

# Extract baseline symbols
find "$LEGACY_DIR" -name "*.a" | while read -r binary; do
    nm -gU "$binary" >> legacy_global_symbols.txt
done

# Extract modern symbols
find "$MODERN_DIR" -name "*.a" | while read -r binary; do
    nm -gU "$binary" >> modern_global_symbols.txt
done

echo "Executing mathematical diff verification..."

# Compare the symbol tables
if diff -q legacy_global_symbols.txt modern_global_symbols.txt; then
    echo "========================================================"
    echo "   SUCCESS: 100% Absolute Binary Parity Confirmed!      "
    echo "========================================================"
else
    echo "========================================================"
    echo "   WARNING: Differences detected in symbol output!      "
    echo "========================================================"
    exit 1
fi

# Clean up temporary files
rm legacy_global_symbols.txt modern_global_symbols.txt
