#!/bin/bash
# Standard Integration Test Runner (AI & Human Friendly)
set -e
SIMULATOR_NAME="iPhone 15"
echo "Booting Simulator: ${SIMULATOR_NAME}..."
xcrun simctl boot "${SIMULATOR_NAME}" || true
echo "Running standard CTest integration suite..."
ctest --preset ios-simulator
