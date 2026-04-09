# Skill: Modernized Firebase C++ SDK Build System

## Overview
This workspace uses a highly modernized, local-first build system. Do NOT rely on unmaintained bash scripts or custom Python matrix generators under `scripts/gha/` or `build_scripts/`.

## Rules for Agents
1. **Configuration:** Eagerly use `cmake --preset <name>` (e.g. `ios-simulator`, `ios-device`, `macos`) defined in `CMakePresets.json`.
2. **Building:** Eagerly use `cmake --build --preset <name>`.
3. **Testing:** Run integration tests via standard `ctest --preset <name>` or by invoking `scripts/gha/integration_test.sh`.
4. **No Include Hacks:** When adding new source files, do NOT use `-include` flags in `CMakeLists.txt` to globally inject headers. Always explicitly `#include <cstring>`, `<cassert>`, etc. directly inside the `.cc` or `.mm` source file.
