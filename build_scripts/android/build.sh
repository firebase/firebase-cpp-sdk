#!/bin/bash -ex

buildpath=$1
sourcepath=$2

if [[ -z "${buildpath}" || -z "${sourcepath}" ]]; then
    echo "Usage: $0 <build path> <source path>"
    exit 1
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
absbuildpath=$( pwd -P )
cd -

# If NDK_ROOT is not set or is the wrong version, use to the version in /tmp.
if [[ -z "${NDK_ROOT}" || ! $(grep -q "Pkg\.Revision = 16\." "${NDK_ROOT}/source.properties") ]]; then
    if [[ ! -d /tmp/android-ndk-r16b ]]; then
	echo "NDK r16b not present in /tmp, please run install_prereqs.sh script."
    fi
    export NDK_ROOT=/tmp/android-ndk-r16b
fi
set -ex
cd "${sourcepath}"
./gradlew assembleRelease
set +x

# Gradle puts the build output inside the source tree, in various
# "build" and ".externalNativeBuild" directories. Grab them and place
# them in the build output directory.
declare -a paths
for lib in *; do
    if [[ -d "${lib}/build" ]]; then
	paths+=("${lib}/build")
    fi
    if [[ -d "${lib}/.externalNativeBuild" ]]; then
	paths+=("${lib}/.externalNativeBuild")
    fi
    if [[ -d "${lib}/${lib}_resources/build" ]]; then
	paths+=("${lib}/${lib}_resources/build")
    fi
    if [[ -d "${lib}/${lib}_java/build" ]]; then
	paths+=("${lib}/${lib}_java/build")
    fi
done
set -x

if [[ $(uname) == "Linux" ]] || [[ $(uname) == "Darwin" ]]; then
  # Turn buildpath into an absolute path for use later with rsync.
  # Use rsync to copy the relevent paths to the destination directory.
  rsync -aR "${paths[@]}" "${absbuildpath}/"
else
  # rsync has to be specifically installed on windows bash (including github runners)
  # Also, rsync with absolute destination path doesn't work on Windows.
  # Using a simple copy instead of rsync on Windows.
  cp -R --parents "${paths[@]}" "${absbuildpath}"
fi
