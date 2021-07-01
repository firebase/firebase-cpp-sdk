#!/bin/bash -e

# Copyright 2020 Google LLC

readonly -a allowed_stl_variants=("c++" "gnustl")
builtpath=$1
packagepath=$2
stl=$3

if [[ -z "${builtpath}" || -z "${packagepath}" || -z "${stl}" ]]; then
    echo "Usage: $0 <built Android SDK path> <path to put packaged files into> <STL variant>"
    echo "STL variant is one of: ${allowed_stl_variants[*]}"
    exit 1
fi

if [[ ! " ${allowed_stl_variants[@]} " =~ " ${stl} " ]]; then
    echo "Invalid STL variant '${stl}'. Allowed STL variants: ${allowed_stl_variants[*]}"
    exit 2
fi

if [[ ! -d "${builtpath}/app/build" ]]; then
    echo "Built Android SDK not found at path '${builtpath}'."
    exit 2
fi

root_dir=$(cd $(dirname $0)/../..; pwd -P)
. "${root_dir}/build_scripts/packaging.conf"
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
# Make sure we only copy the libraries in product_list (specified in packaging.conf)
for product in ${product_list[*]}; do
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
    # Copy the special messaging aar file, but only if messaging was built.
    if [[ "${product}" == "messaging" ]]; then
        cp -f "${sourcepath}/messaging/messaging_java/build/outputs/aar/messaging_java-release.aar" "${destpath}/libs/android/firebase_messaging_cpp.aar"
    fi
done
cd "${origpath}"
