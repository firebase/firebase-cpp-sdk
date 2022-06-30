#!/bin/bash -e

# Copyright 2022 Google LLC
#

PLATFORM='Desktop'
RUNNING_ONLY=''
ARCH=''
SSL=''
# check options
while getopts ":p:a:r:s" opt; do
    case $opt in
        p)
            PLATFORM=$OPTARG
            ;;
        a)
            ARCH=$OPTARG
            ;;
        r)
            if [[ $OPTARG == true ]]; then
                RUNNING_ONLY="--running_only"
            fi
            ;;
        s)
            SSL=$OPTARG
            ;;
        *)
            echo "unknown parameter"
            echo $OPTARG
            exit 2
            ;;
    esac
done

OS=''
case "$OSTYPE" in
  darwin*)  OS='Mac' ;; 
  linux*)   OS='Linux' ;;
  msys*)    OS='Windows' ;;
  *)        echo "unknown: $OSTYPE" ;;
esac

git config --global credential.helper 'store --file /tmp/git-credentials'
echo 'https://${{ github.token }}@github.com' > /tmp/git-credentials

if [[ "${OS}" == "Windows" ]]; then
    git config --system core.longpaths true
fi

if [[ "${OS}" == "Mac" ]]; then
    sudo xcode-select -s /Applications/Xcode_${ fromJson(needs.check_and_prepare.outputs.xcode_version)[0] }.app/Contents/Developer
    brew update
fi

if [[ "${PLATFORM}" == "Desktop" ]]; then
    if [[ "${OS}" == "Linux" ]]; then
        VCPKG_TRIPLET="x64-linux"
    elif [[ "${OS}" == "Mac" ]]; then
        VCPKG_TRIPLET="x64-osx"
    elif [[ "${OS}" == "Windows" ]]; then
        VCPKG_TRIPLET="x64-windows-static"
    fi
    echo "VCPKG_TRIPLET=${VCPKG_TRIPLET}" >> $GITHUB_ENV
    echo "VCPKG_RESPONSE_FILE=external/vcpkg_${VCPKG_TRIPLET}_response_file.txt" >> $GITHUB_ENV
    python install_prereqs_desktop.py --gha_build ${RUNNING_ONLY} --arch ${ARCH} --ssl ${SSL}
fi

if [[ "${RUNNING_ONLY}" == "" ]]; then
    if [[ "${PLATFORM}" == "Android" ]]; then
        echo "NDK_ROOT=/tmp/android-ndk-r21e" >> $GITHUB_ENV
        echo "ANDROID_NDK_HOME=/tmp/android-ndk-r21e" >> $GITHUB_ENV
        ../../build_scripts/android/install_prereqs.sh
    elif [[ "${PLATFORM}" == "iOS" ]]; then
        ../../build_scripts/ios/install_prereqs.sh
    elif [[ "${PLATFORM}" == "tvOS" ]]; then
        ../../build_scripts/tvos/install_prereqs.sh
    fi
fi
