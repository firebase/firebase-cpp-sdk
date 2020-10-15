#!/bin/bash -e

buildpath=$1
sourcepath=$2
arch=$3
format=$4

if [[ -z "${buildpath}" || -z "${sourcepath}" || -z "${arch}" ]]; then
    echo "Usage: $0 <build path> <source path> <arm64|x86_64> [frameworks|libraries]"
    exit 1
fi

if [[ "${arch}" == "x86_64" ]]; then
    toolchain="cmake/toolchains/ios_simulator.cmake"
elif [[ "${arch}" == "arm64" ]]; then
    toolchain="cmake/toolchains/ios.cmake"
else
    echo "Invalid architecture: '${arch}'"
    exit 2
fi

if [[ "${format}" == "" ]]; then
    format="frameworks"
fi

if [[ "${format}" != "libraries" && "${format}" != "frameworks" ]]; then
    echo "Invalid target format specified."
    echo "Valid formats are: 'frameworks' (default) or 'libraries'"
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

cmake -GXcode "-DCMAKE_TOOLCHAIN_FILE=${toolchain}" "-DTARGET_FORMAT=${format}" -S "${sourcepath}" -B "${buildpath}"
cmake --build "${buildpath}"
