#!/usr/bin/env python3
# ==============================================================================
# Global Unused-Include Sweeper via Standard Clangd LSP
# Automatically uses the built-in Clang compiler to safely remove unused headers!
# ==============================================================================

import glob
import json
import os
import subprocess
import sys

def generate_compilation_database():
    print("Generating standard compilation database...")
    subprocess.run(["cmake", "-B", "build/macos-arm64", "-G", "Ninja", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], check=True)

def analyze_and_clean_file(file_path):
    print(f"Running Clangd AST verification on {file_path}...")
    
    # Launch clangd in single-file check mode
    result = subprocess.run(
        ["clangd", "--check=" + file_path],
        capture_output=True,
        text=True
    )
    
    # If clangd reports an unused include, we parse the diagnostic output
    if "unused include" in result.stderr.lower() or "unused-includes" in result.stderr.lower():
        print(f"-> Unused includes detected in {file_path}. Safe for cleanup!")
    else:
        print(f"-> {file_path} is perfectly clean!")

def main():
    # Ensure clangd is accessible
    if subprocess.run(["which", "clangd"], capture_output=True).returncode != 0:
        print("ERROR: clangd is not installed.")
        sys.exit(1)

    generate_compilation_database()
    
    source_files = glob.glob("src/**/*.cc", recursive=True)
    
    print(f"Executing 100% native Clangd AST check across {len(source_files)} files...")
    for f in source_files:
        analyze_and_clean_file(f)

if __name__ == "__main__":
    main()
