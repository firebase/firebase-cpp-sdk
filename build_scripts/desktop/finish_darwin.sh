#!/bin/bash

# Copyright 2020 Google LLC
#
# Script to finish packaging individual iOS/Mac libraries into
# universal libraries, and then into frameworks.
set -e

if [[ "$(uname)" != "Darwin" ]]; then
    echo "Error: Frameworks and universal libraries can only be built on a Mac host."
    exit 2
fi

packagepath=$1
includepath=$2
binutils=$3
os=$4

if [[ -z "${os}" ]]; then
    os=darwin
fi

if [[ -z "${packagepath}" || -z "${includepath}" || -z "${binutils}" ]]; then
    echo "Usage: $0 <packaged iOS/Mac SDK path> <include file path> <binutils path> [darwin|ios]"
    exit 2
fi

if [[ ! -d "${packagepath}/libs/${os}" ]]; then
    echo "Can't find packaged iOS/Mac SDK in ${packagedpath}/libs/${os}"
fi

if [[ ! -r "${includepath}/firebase/app.h" ]]; then
    echo "Can't find include files in ${includepath}"
    exit 2
fi

if [[ ! -x "${binutils}/ar" ]]; then
    echo "Packaging tools not found at path '${binutils}'."
    exit 2
fi

origpath=$(pwd -P)
packagepath=$(cd ${packagepath}; pwd -P)
includepath=$(cd ${includepath}; pwd -P)
binutils=$(cd ${binutils}; pwd -P)

declare -a architectures
for a in $(cd "${packagepath}"/libs/"${os}"; find . -depth 1 -type d | sed 's|^\./||'); do
    if [[ $a != "universal" ]]; then
	architectures+=($a)
    fi
done

# Don't lipo if we already have universal libraries, as that
# means that the lipo step has already occurred.
if [[ ! -d "${packagepath}/libs/${os}/universal" ]]; then
    echo "Repackaging libraries using Mac libtool..."
    for arch in ${architectures[*]}; do
	for lib in "${packagepath}/libs/${os}/${arch}"/*.a; do
	    pushd $(dirname ${lib}) > /dev/null
	    libname=$(basename "${lib}")
	    mkdir "${libname}.dir"
	    cd "${libname}.dir"
	    "${binutils}"/ar x ../"${libname}"
	    mv -f ../"${libname}"  ../"${libname}.bak"
	    xcrun libtool -static -o "../${libname}" -no_warning_for_no_symbols -s *.o
	    cd ..
	    rm -rf "${libname}.dir"
	    popd > /dev/null
	done
    done

    # Merge into universal libs
    mkdir "${packagepath}/libs/${os}/universal"
    echo "Creating universal libraries..."
    for lib in "${packagepath}/libs/${os}/${architectures[0]}"/*.a; do
	    libname=$(basename ${lib})
	    xcrun lipo "${packagepath}/libs/${os}"/*/"${libname}" -create -output "${packagepath}/libs/${os}/universal/${libname}"
    done

    # Done, remove old library backups.
    rm -f "${packagepath}/libs/${os}"/*/*.bak

else
    echo "Universal libraries already found, skipping..."
fi

# Now make frameworks.
echo "Creating frameworks..."
rm -rf "${packagepath}/frameworks/${os}"
mkdir -p "${packagepath}/frameworks/${os}"
for arch in universal ${architectures[*]}; do
    for f in "${packagepath}/libs/${os}/${arch}/"*.a; do
	library=$f
	framework=$(basename "${library}" | sed 's|^lib||' | sed 's|\.a$||')
	add_headers=0
	if [[ "${framework}" == "firebase_app" ]]; then
	    framework="firebase"
	    add_headers=1
	    echo "Need to add extra Headers"
	fi
	framework_dir="${packagepath}/frameworks/${os}/${arch}/${framework}.framework"
	mkdir -p "${framework_dir}"
	cp -f "${library}" "${framework_dir}/${framework}"
	if [[ ${add_headers} -eq 1 ]]; then
	    echo "Adding extra Headers to framework ${framework_dir}"
	    mkdir "${framework_dir}/Headers"
	    cp -af "${includepath}/firebase/"* "${framework_dir}/Headers/"
	    cp -af "${packagepath}/include/firebase/"* "${framework_dir}/Headers/"
	fi
    done
done
echo "Done!"
