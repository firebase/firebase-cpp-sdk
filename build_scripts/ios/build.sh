#!/bin/bash

buildpath=$1
sourcepath=$2
arch=$3

if [[ -z "${buildpath}" || -z "${sourcepath}" || -z "${arch}" ]]; then
    echo "Usage: $0 <build path> <source path> <arm64|x86_64>"
    exit 1
fi

if [[ "${arch}" == "x86_64" ]]; then
    toolchain="${sourcepath}/cmake/toolchains/ios_simulator.cmake"
elif [[ "${arch}" == "arm64" ]]; then
    toolchain="${sourcepath}/cmake/toolchains/ios.cmake"
else
    echo "Invalid architecture: '${arch}'"
    exit 2
fi

if [[ ! -d "${sourcepath}" ]]; then
    echo "Source path '${sourcepath}' not found."
    exit 2
fi

mkdir -p "${buildpath}"
cd "${buildpath}"
if [[ -n $(ls) ]]; then
    echo "Error: build path '${buildpath}' not empty."
    exit 2
fi
cd -

set -ex

cmake "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -S "${sourcepath}" -B "${buildpath}"
cmake --build "${buildpath}"
