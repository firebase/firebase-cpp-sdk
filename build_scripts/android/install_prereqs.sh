#!/bin/bash -e

# Copyright 2020 Google LLC

if [[ $(uname) == "Darwin" ]]; then
    platform=darwin
    if [[ ! -z "${GHA_INSTALL_CCACHE}" ]]; then
        brew install ccache
        echo "CCACHE_INSTALLED=1" >> $GITHUB_ENV
    fi
elif [[ $(uname) == "Linux" ]]; then
    platform=linux
    if [[ ! -z "${GHA_INSTALL_CCACHE}" ]]; then
        sudo apt install ccache
        echo "CCACHE_INSTALLED=1" >> $GITHUB_ENV
    fi
else
    platform=windows
    # On Windows, we have an additional dependency for Strings
    set +e
    # Retry up to 10 times because Curl has a tendency to timeout on
    # Github runners.
    for retry in {1..10} error; do
	if [[ $retry == "error" ]]; then exit 5; fi
	curl -LSs \
             "https://download.sysinternals.com/files/Strings.zip" \
             --output Strings.zip && break
	sleep 300
    done
    set -e
    unzip -q Strings.zip && rm -f Strings.zip
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
      # On Windows bash shell, sudo doesn't exist
      if [[ $(uname) == "Linux" ]] || [[ $(uname) == "Darwin" ]]; then
        sudo python -m pip install --upgrade pip
      else
        python -m pip install  --upgrade pip
      fi
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
	    set +e
	    # Retry up to 10 times because Curl has a tendency to timeout on
	    # Github runners.
	    for retry in {1..10} error; do
	        if [[ $retry == "error" ]]; then exit 5; fi
	        curl --http1.1 -LSs \
		    "https://dl.google.com/android/repository/android-ndk-r16b-${platform}-x86_64.zip" \
		    --output /tmp/android-ndk-r16b.zip && break
	        sleep 300
	    done
	    set -e
	    (cd /tmp && unzip -q android-ndk-r16b.zip && rm -f android-ndk-r16b.zip)
	    echo "NDK r16b has been downloaded into /tmp/android-ndk-r16b"
    fi
fi
