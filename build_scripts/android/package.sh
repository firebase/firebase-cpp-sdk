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

mkdir -p "${destpath}/libs/android"

# Copy each platform's libraries to the destination directory for this STL variant.
cd "${sourcepath}"
for product in *; do
    if [[ ! -d "${product}/build/intermediates/cmake/release/obj" ]]; then
	continue
    fi
    dir="${product}/build/intermediates/cmake/release/obj"
    for cpudir in "${dir}"/*; do
	cpu=$(basename ${cpudir})
	libsrc="${sourcepath}/${cpudir}/libfirebase_${product}.a"
	libdest="${destpath}/libs/android/${cpu}/${stl}"
	mkdir -p "${libdest}"
	cp -f "${libsrc}" "${libdest}/"
    done
    # Copy the top-level Proguard files in as well.
    cp -f "${sourcepath}/${product}/build/Release/${product}.pro" "${destpath}/libs/android/"
done
cd "${origpath}"
