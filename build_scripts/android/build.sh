#!/bin/bash -e

# Copyright 2020 Google LLC

buildpath=$1
sourcepath=$2

if [[ -z "${buildpath}" || -z "${sourcepath}" ]]; then
    echo "Usage: $0 <build path> <source path> [c++]"
    exit 1
fi

if [[ ! -d "${sourcepath}" ]]; then
    echo "Source path '${sourcepath}' not found."
    exit 2
fi

origpath=$( pwd -P )

mkdir -p "${buildpath}"
cd "${buildpath}"
if [[ -n $(ls) ]]; then
    echo "Error: build path '${buildpath}' not empty."
    exit 2
fi
absbuildpath=$( pwd -P )
cd "${origpath}"

# If NDK_ROOT is not set or is the wrong version, use to the version in /tmp.
if [[ -z "${NDK_ROOT}" || -z $(grep "Pkg\.Revision = 21\." "${NDK_ROOT}/source.properties") ]]; then
    if [[ ! -d /tmp/android-ndk-r21e ]]; then
        echo "Recommended NDK version r21e not present in /tmp."
        echo "Please run install_prereqs.sh if you wish to use the recommended NDK version."
        echo "Continuing with default NDK..."
        sleep 2
    fi
    export NDK_ROOT=/tmp/android-ndk-r21e
    export ANDROID_NDK_HOME=/tmp/android-ndk-r21e
fi
cd "${sourcepath}"
set +e
# Retry the build up to 10 times, because the build fetches files from
# maven and elsewhere, and occasionally the GitHub runners have
# network connectivity issues that cause the download to fail.
gradleparams="-Dhttp.keepAlive=false -Dmaven.wagon.http.pool=false\
 -Dmaven.wagon.httpconnectionManager.ttlSeconds=120"
for retry in {1..10} error; do
    if [[ $retry == "error" ]]; then exit 5; fi
    ./gradlew assembleRelease "${gradleparams}" && break
    sleep 300
done
set -e

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
