#!/bin/bash -e

builtpath=$1
packagepath=$2

if [[ -z "${builtpath}" || -z "${packagepath}" ]]; then
    echo "Usage: $0 <built iOS SDK path> <path to put packaged files into>"
    exit 1
fi

if [[ ! -d "${builtpath}/frameworks" ]]; then
    echo "Built iOS SDK not found at path '${builtpath}'."
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

# Copy frameworks into packaged SDK.
cd "${sourcepath}"
for arch_dir in frameworks/ios/*; do
    mkdir -p "${destpath}/${arch_dir}"
    # Make sure we only copy the frameworks in product_list (specified in packaging.conf)
    for product in ${product_list[*]}; do
	if [[ "${product}" == "app" ]]; then
	    framework_dir="firebase.framework"
	else
	    framework_dir="firebase_${product}.framework"
	fi
	cp -af "${arch_dir}/${framework_dir}" "${destpath}/${arch_dir}/${framework_dir}"
    done
done
cd "${origpath}"

# Convert frameworks into libraries so we can provide both in the SDK.
cd "${destpath}"
for frameworkpath in frameworks/ios/*/*.framework; do
    libpath=$(echo "${frameworkpath}" | sed 's|^frameworks|libs|' | sed 's|\([^/]*\)\.framework$|lib\1.a|')
    if [[ $(basename "${libpath}") == 'libfirebase.a' ]]; then
	libpath=$(echo "${libpath}" | sed 's|libfirebase\.a|libfirebase_app.a|')
    fi

    frameworkpath=$(echo "${frameworkpath}" | sed 's|\([^/]*\)\.framework$|\1.framework/\1|')
    mkdir -p $(dirname "${destpath}/${libpath}")
    cp -af "${destpath}/${frameworkpath}" "${destpath}/${libpath}"
done
cd "${origpath}"
