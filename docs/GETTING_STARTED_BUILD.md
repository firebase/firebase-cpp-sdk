# Getting Started: 1-Step Build System

We have provided an incredibly simple, automated orchestrator that lets you configure the platform and compile your exact sub-SDK module(s) in a **single step** without needing to type out prefixes!

---

## 1. Building Specific Targets
Open your terminal and run:

```bash
./scripts/gha/build.py --preset <Platform> --target <Module1> <Module2> ...
```

### Examples:
```bash
# Build Auth and Firestore for Android simultaneously (No prefix required!):
./scripts/gha/build.py --preset android-arm64 --target auth firestore

# Build Remote Config for macOS instantly:
./scripts/gha/build.py --preset macos-arm64 --target remote_config
```

---

## 2. Building the Entire SDK (Local & CI)
To build every single module and binary in the workspace simultaneously, run the exact same command **without passing a target**:

### Local Workstation Build:
```bash
# Build everything for macOS Apple Silicon instantly:
./scripts/gha/build.py --preset macos-arm64

# Build everything for Android arm64 instantly:
./scripts/gha/build.py --preset android-arm64
```

### GitHub Actions Integration:
Because the orchestrator provides absolute 1:1 local reproducibility, your CI pipelines invoke the exact same command:

```yaml
jobs:
  build-macos:
    runs-on: macos-14
    steps:
      - uses: actions/checkout@v4
      - name: Build Entire Firebase SDK
        run: ./scripts/gha/build.py --preset macos-arm64
```
