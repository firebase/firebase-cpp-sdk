#!/usr/bin/env python3
"""
Standard Build Orchestrator for Firebase C++ SDK.
Provides a perfect 1-step command combining platform selection, target compilation, and native XCFramework assembly using the Modern Xcode Build System!
"""

import argparse
import glob
import os
import subprocess
import sys

def run_command(cmd):
    print(f"Executing: {' '.join(cmd)}")
    subprocess.run(cmd, check=True)

def assemble_xcframework(build_dir):
    frameworks = glob.glob(os.path.join(build_dir, "**", "*.framework"), recursive=True)
    
    if not frameworks:
        print("No .framework files found to bundle into an .xcframework.")
        return

    for framework in frameworks:
        base_name = os.path.basename(framework).replace(".framework", "")
        xcframework_path = os.path.join(build_dir, f"{base_name}.xcframework")
        
        if os.path.exists(xcframework_path):
            subprocess.run(["rm", "-rf", xcframework_path])
            
        print(f"Assembling native XCFramework for {base_name}...")
        run_command([
            "xcodebuild", 
            "-create-xcframework", 
            "-framework", framework, 
            "-output", xcframework_path
        ])

def main():
    parser = argparse.ArgumentParser(description="1-Step Build Orchestrator for Firebase C++")
    parser.add_argument("--preset", default="ios-sim-universal", help="Target platform architecture (e.g., ios-arm64, macos-arm64)")
    parser.add_argument("--target", nargs="+", default=[], help="Specific module(s) to compile (e.g., auth firestore)")
    args = parser.parse_args()

    # Step 1: Configure
    run_command(["cmake", "--preset", args.preset])
    
    # Step 2: Compile using the fully standard Modern Xcode Build System
    build_cmd = ["cmake", "--build", "--preset", args.preset]
    
    for t in args.target:
        actual_target = t if t.startswith("firebase_") else f"firebase_{t}"
        build_cmd.extend(["--target", actual_target])
        
    run_command(build_cmd)
    
    # Step 3: Assemble XCFramework
    if "ios" in args.preset or "macos" in args.preset:
        binary_dir = f"./build/{args.preset}"
        if os.path.exists(binary_dir):
            assemble_xcframework(binary_dir)

if __name__ == "__main__":
    main()
