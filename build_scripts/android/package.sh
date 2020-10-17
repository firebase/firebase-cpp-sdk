#!/bin/bash -e

builtpath=$1
packagepath=$2
stl=$3

if [[ -z "${builtpath}" || -z "${packagepath}" || -z "${stl}" ]]; then
    echo "Usage: $0 <built Android SDK path> <path to put packaged files into> <STL variant>"
    echo "STL variant is one of: c++ gnustl stlport"
    exit 1
fi

if [[ ! -d "${builtpath}/" ]]; then
    echo "Built iOS SDK not found at path '${builtpath}'."
    exit 2
fi

origpath=$( pwd -P )

mkdir -p "${packagepath}"
cd "${packagepath}"
destpath=$( pwd -P )
cd "${origpath}"

cd "${builtpath}"
sourcepath=$( pwd -P )
cd "${origpath}"

