#!/bin/bash -e

# Copyright 2022 Google LLC

PLATFORM='Desktop'
TEST_ONLY=''
ARCH=''
SSL=''
# check options
while getopts ":p:a:t:s:" opt; do
    case $opt in
        p)
            PLATFORM=$OPTARG
            ;;
        a)
            ARCH=$OPTARG
            ;;
        t)
            if [[ $OPTARG == true ]]; then
                TEST_ONLY="--running_only"
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

echo "PLATFORM: ${PLATFORM}"
echo "ARCH: ${ARCH}"
echo "TEST_ONLY: ${TEST_ONLY}"
echo "SSL: ${SSL}"

OS=''
case "$OSTYPE" in
  darwin*)  OS='Mac' ;; 
  linux*)   OS='Linux' ;;
  msys*)    OS='Windows' ;;
  *)        echo "unknown: $OSTYPE" ;;
esac
echo "OS: ${OS}"

pip install -r scripts/gha/python_requirements.txt

git config --global credential.helper 'store --file /tmp/git-credentials'
echo 'https://${GITHUB_TOKEN}@github.com' > /tmp/git-credentials

if [[ "${OS}" == "Windows" ]]; then
    git config --system core.longpaths true
fi

if [[ "${OS}" == "Mac" ]]; then
    sudo xcode-select -s /Applications/Xcode_${xcodeVersion}.app/Contents/Developer
fi

if [[ "${PLATFORM}" == "Desktop" ]]; then
    if [[ "${OS}" == "Linux" ]]; then
        VCPKG_TRIPLET="x64-linux"
    elif [[ "${OS}" == "Mac" ]]; then
        VCPKG_TRIPLET="x64-osx"
        echo "OPENSSL_ROOT_DIR=/usr/local/opt/openssl@1.1" >> $GITHUB_ENV
    elif [[ "${OS}" == "Windows" ]]; then
        VCPKG_TRIPLET="x64-windows-static"
    fi
    echo "VCPKG_TRIPLET=${VCPKG_TRIPLET}" >> $GITHUB_ENV
    echo "VCPKG_RESPONSE_FILE=external/vcpkg_${VCPKG_TRIPLET}_response_file.txt" >> $GITHUB_ENV
    python scripts/gha/install_prereqs_desktop.py --gha_build --arch "${ARCH}" --ssl "${SSL}" ${TEST_ONLY} 
fi

if [[ "${TEST_ONLY}" == "" ]]; then
    if [[ "${PLATFORM}" == "Android" ]]; then
        echo "NDK_ROOT=/tmp/android-ndk-r21e" >> $GITHUB_ENV
        echo "ANDROID_NDK_HOME=/tmp/android-ndk-r21e" >> $GITHUB_ENV
        build_scripts/android/install_prereqs.sh
    elif [[ "${PLATFORM}" == "iOS" ]]; then
        build_scripts/ios/install_prereqs.sh
    elif [[ "${PLATFORM}" == "tvOS" ]]; then
        build_scripts/tvos/install_prereqs.sh
    fi
fi
