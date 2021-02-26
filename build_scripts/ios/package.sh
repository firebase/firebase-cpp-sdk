#!/bin/bash -e

builtpath=$1
packagepath=$2

if [[ -z "${builtpath}" || -z "${packagepath}" ]]; then
    echo "Usage: $0 <built iOS SDK path> <path to put packaged files into>"
    exit 1
fi

if [[ ! -d "${builtpath}/xcframeworks" ]]; then
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

# Copy xcframeworks into packaged SDK.
cd "${sourcepath}"
mkdir -p "${destpath}/xcframeworks"
# Make sure we only copy the xcframeworks in product_list (specified in packaging.conf)
for product in ${product_list[*]}; do
	if [[ "${product}" == "app" ]]; then
	    framework_dir="firebase.xcframework"
	else
	    framework_dir="firebase_${product}.xcframework"
	fi
	cp -R ${sourcepath}/xcframeworks/${framework_dir} ${destpath}/xcframeworks/
done
cd "${origpath}"

# Convert frameworks into libraries so we can provide both in the SDK.
cd "${sourcepath}"
for frameworkpath in frameworks/ios/*/*.framework; do
    libpath=$(echo "${frameworkpath}" | sed 's|^frameworks|libs|' | sed 's|\([^/]*\)\.framework$|lib\1.a|')
    if [[ $(basename "${libpath}") == 'libfirebase.a' ]]; then
	libpath=$(echo "${libpath}" | sed 's|libfirebase\.a|libfirebase_app.a|')
    fi

    frameworkpath=$(echo "${frameworkpath}" | sed 's|\([^/]*\)\.framework$|\1.framework/\1|')
    mkdir -p $(dirname "${destpath}/${libpath}")
    cp -af "${sourcepath}/${frameworkpath}" "${destpath}/${libpath}"
done
cd "${origpath}"
