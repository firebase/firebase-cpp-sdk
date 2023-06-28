#!/bin/bash -e

# Copyright 2020 Google LLC

if [[ $(uname) != "Darwin" ]]; then
    echo "Unsupported platform, iOS can only be build on a MacOS machine."
    exit 1
fi

if [[ -z $(which cmake) ]]; then
    echo "Error, cmake is not installed or is not in the PATH."
    exit 1
fi

python=python
if [[ -z $(which ${python}) ]]; then
    python=python3
    if [[ -z $(which ${python}) ]]; then
      echo "Error, python is not installed as python or python3, or is not in the PATH."
      exit 1
    fi
fi

updated_pip=0
if ! $(echo "import absl"$'\n'"import google.protobuf" | ${python} - 2> /dev/null); then
    echo "Installing python packages."
    set -x
    sudo ${python} -m pip install --upgrade pip
    pip install absl-py protobuf
    set +x
fi


if [[ -z $(which xcodebuild) || -z $(which xcode-select) ]]; then
    echo "Error, Xcode command line tools not installed."
    exit 1
fi

if [[ -z $(xcode-select -p) || ! -d  $(xcode-select -p) ]]; then
    echo "Error, no Xcode version selected. Use 'xcode-select' to select one."
    exit 1
fi

if [[ -z $(which pod) ]]; then
    echo "Cocoapods not detected, installing..."
    sudo gem install cocoapods
fi

echo "Updating Cocoapods repo..."
pod repo update

