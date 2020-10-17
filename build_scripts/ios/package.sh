#!/bin/bash -e

builtpath=$1
packagepath=$2

if [[ -z "${builtpath}" || -z "${packagepath}" ]]; then
    echo "Usage: $0 <built SDK path> <path to package into>"
    exit 1
fi

if [[ ! -d "${builtpath}" ]]; then
    echo "Source path '${builtpath}' not found."
    exit 2
fi

origpath=$( pwd -P )

mkdir -p "${packagepath}"
cd "${packagepath}"
if [[ -n $(ls) ]]; then
    echo "Error: destination package path '${packagepath}' not empty."
    exit 2
fi
destpath=$( pwd -P )
cd "${origpath}"

cd "${builtpath}"
sourcepath=$( pwd -P )
cd "${origpath}"

# Copy frameworks into packaged SDK.
mkdir -p "${destpath}/frameworks/ios/"
cp -av "${sourcepath}/frameworks/ios/" "${destpath}/frameworks/ios"

# Convert frameworks into libraries so we can provide both in the SDK.
cd "${sourcepath}"
for frameworkpath in frameworks/ios/*/*.framework; do
    libpath=$(echo "${frameworkpath}" | sed 's|^frameworks|libs|' | sed 's|\([^/]*\)\.framework$|lib\1.a|')
    if [[ $(basename "${libpath}") == 'libfirebase.a' ]]; then
	libpath=$(echo "${libpath}" | sed 's|libfirebase\.a|libfirebase_app.a|')
    fi
	
    frameworkpath=$(echo "${frameworkpath}" | sed 's|\([^/]*\)\.framework$|\1.framework/\1|')
    mkdir -p $(dirname "${destpath}/${libpath}")
    cp -av "${destpath}/${frameworkpath}" "${destpath}/${libpath}"
done
cd "${origpath}"
