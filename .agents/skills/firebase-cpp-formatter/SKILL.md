---
name: firebase-cpp-formatter
description:
  Formatting and linting workflow for C++ and Objective-C code in the Firebase
  C++ SDK. Use when modifying source code and ensuring compliance with the
  repository's clang-format rules and cpplint guidelines.
---

# Code Formatting & Linting for Firebase C++ SDK

This skill provides instructions on how to properly format and lint C++ and
Objective-C code before creating a pull request or committing changes in the
`firebase-cpp-sdk` repository.

## Prerequisites

Before running the formatting script, ensure that your Python environment is set up with all required dependencies:

- **Python Dependencies**: Run `pip install -r scripts/gha/python_requirements.txt` to install the required libraries (such as `attrs`, `absl-py`, etc.).
- **cpplint**: Run `pip install cpplint` to install the C++ linter.

## Formatting Workflows

### 1. Formatting Specific Changed Files (Git Diff)

If you have made code changes locally in a git branch, the easiest way to ensure
all your changes are formatted properly is to use the `-git_diff` argument. This
automatically detects which files have changed compared to `main` and formats
them.

```bash
python3 scripts/format_code.py -git_diff
```

_Tip:_ You can also append `-verbose` for extensive logging about formatting
changes.

### 2. Formatting a Specific Directory

If you are working on a feature within a particular Firebase SDK (e.g.,
`database`), you can recursively format that entire directory:

```bash
python3 scripts/format_code.py -d database -r
```

To target multiple directories (e.g., both `database` and `app`):

```bash
python3 scripts/format_code.py -d database -d app -r
```

### 3. Formatting Specific Files

If you are pressed for time and only want to format specific files rather than
analyzing whole directories, use the `-f` flag:

```bash
python3 scripts/format_code.py -f path/to/file1.cc -f path/to/file2.h
```

### 4. Dry Run (Check without Formatting)

If you only want to detect the number of files that _need_ formatting without
actually modifying them, append the `--noformat_file` flag:

```bash
python3 scripts/format_code.py -git_diff --noformat_file
```

### 5. Formatting Nuances (Short Blocks & Braces)

The repository's CI runs on Ubuntu 22.04 with `clang-format 14`. Under Google style with this configuration, single-statement short blocks are formatted on a single line:
```cpp
{ SleepUponDestruction sleep_for_listeners; }
```
Splitting such statements across three lines may cause the `file_format_check` CI step to fail. Always run `python3 scripts/format_code.py -git_diff` prior to committing to ensure your formatting exactly matches CI expectations.

## Code Linting (`cpplint`)

The repository runs `cpplint` on all changed files in pull requests via GitHub Actions. You can run the linter locally using the repository's configured filter set:

```bash
python3 -m cpplint --filter=-build/include_subdir,-build/c++11,-readability/casting,-runtime/references,-whitespace/indent,-whitespace/line_length <files>
```

Common lint violations to look out for:
- `[runtime/int]`: Using plain C `long` or `unsigned long long`. Use `int64_t` or `uint64_t` instead.
- `[build/include_what_you_use]`: Missing direct `#include` directives for standard headers like `<map>`, `<vector>`, `<string>`, or `<cstdint>`.
