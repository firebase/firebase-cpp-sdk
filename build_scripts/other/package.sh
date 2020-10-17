#!/bin/bash -e

sdkpath=$1
packagepath=$2

if [[ -z "${sdkpath}" || -z "${packagepath}" ]]; then
    echo "Usage: $0 <SDK source path> <path to put packaged files into>"
    exit 1
fi

if [[ ! -d "${sdkpath}/release_build_files" ]]; then
    echo "SDK source not found at '${sdkpath}'."
    exit 2
fi

origpath=$( pwd -P )

mkdir -p "${packagepath}"
cd "${packagepath}"
destpath=$( pwd -P )
cd "${origpath}"

cd "${sdkpath}"
sourcepath=$( pwd -P )
cd "${origpath}"

# Copy headers to packaged SDK.
echo mkdir -p "${destpath}/include"
cd "${sdkpath}"
for incdir in */src/include/; do
    cp -av "${incdir}" "${destpath}/include/"
done
cd "${origpath}"

# Copy release files into packaged SDK.
cp -av "${sourcepath}/release_build_files/" "${destpath}/"

# Copy generate_xml tool into packaged SDK.
cp -v "${sourcepath}/generate_xml_from_google_services_json.py" "${destpath}/"
cp -v "${sourcepath}/generate_xml_from_google_services_json.exe" "${destpath}/"

