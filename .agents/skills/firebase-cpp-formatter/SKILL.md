---
name: firebase-cpp-formatter
description:
  Formatting workflow for C++ and Objective-C code in the Firebase C++ SDK. Use
  when modifying source code and ensuring compliance with the repository's
  clang-format rules.
---

# Code Formatting for Firebase C++ SDK

This skill provides instructions on how to properly format C++ and Objective-C
code before creating a pull request or committing changes in the
`firebase-cpp-sdk` repository. The primary tool is `scripts/format_code.py`.

## Workflows

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
actually modifying them, append the `-fnoformat_file` flag:

```bash
python3 scripts/format_code.py -git_diff -fnoformat_file
```
