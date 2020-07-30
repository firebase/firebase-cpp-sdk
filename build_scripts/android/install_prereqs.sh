#!/bin/bash -e

if [[ $(uname) == "Darwin" ]]; then
    platform=darwin
    if [[ ! -z "${GHA_INSTALL_CCACHE}" ]]; then
        brew install ccache
        echo "::set-env name=CCACHE_INSTALLED::1"
    fi
elif [[ $(uname) == "Linux" ]]; then
    platform=linux
    if [[ ! -z "${GHA_INSTALL_CCACHE}" ]]; then
        sudo apt install ccache
        echo "::set-env name=CCACHE_INSTALLED::1"
    fi
else
    echo "Unsupported platform, this script must run on a MacOS or Linux machine."
    exit 1
fi

if [[ -z $(which cmake) ]]; then
    echo "Error, cmake is not installed or is not in the PATH."
    exit 1
fi

if [[ -z $(which python) ]]; then
    echo "Error, python is not installed or is not in the PATH."
    exit 1
else
    updated_pip=0
    if ! $(echo "import absl"$'\n'"import google.protobuf" | python - 2> /dev/null); then
	echo "Installing python packages."
	set -x
	sudo python -m pip install --upgrade pip
	pip install absl-py protobuf
	set +x
    fi
fi

if [[ -z "${ANDROID_HOME}" ]]; then
    echo "Error, ANDROID_HOME environment variable is not set."
    exit 1
fi

if [[ -z "${NDK_ROOT}" || -z $(grep "Pkg\.Revision = 16\." "${NDK_ROOT}/source.properties") ]]; then
    if [[ -d /tmp/android-ndk-r16b && \
	      -n $(grep "Pkg\.Revision = 16\." "/tmp/android-ndk-r16b/source.properties") ]]; then
	echo "Using NDK r16b in /tmp/android-ndk-r16b".
    else
       echo "NDK_ROOT environment variable is not set, or NDK version is incorrect."
        echo "This build requires NDK r16b for STLPort compatibility, downloading..."
	if [[ -z $(which curl) ]]; then
	    echo "Error, could not run 'curl' to download NDK. Is it in your PATH?"
	    exit 1
	fi
	set -x
	curl -LSs \
	     "https://dl.google.com/android/repository/android-ndk-r16b-${platform}-x86_64.zip" \
	     --output /tmp/android-ndk-r16b.zip
	(cd /tmp && unzip -q android-ndk-r16b.zip && rm -f android-ndk-r16b.zip)
	set +x
	echo "NDK r16b has been downloaded into /tmp/android-ndk-r16b"
    fi
fi

if [[ ! -z "${INSTALL_CCACHE}" ]]; then
    if ; then
        echo "mac!"
    else
        echo "linux!"
    fi
fi
    

