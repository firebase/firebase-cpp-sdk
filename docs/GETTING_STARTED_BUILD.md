# Getting Started: 1-Step Build System

We have provided incredibly simple, automated tools that let you compile specific modules, full individual SDK platforms, or the entire universal multi-architecture ecosystem instantly!

---

## 1. Master "Build Everything" Command
To automatically run a complete release-style compilation across all major supported platforms (iOS Device, iOS Simulator, macOS, Android, and Linux) simultaneously, open your terminal and run:

```bash
./scripts/gha/build_everything.sh
```

---

## 2. Building Specific Targets
If you only want to build for a single platform:

```bash
./scripts/gha/build.py --preset <Platform> --target <Module1> <Module2> ...
```

### Examples:
```bash
# Build Auth and Firestore for Android simultaneously:
./scripts/gha/build.py --preset android-arm64 --target auth firestore
```
