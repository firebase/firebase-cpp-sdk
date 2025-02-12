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

if [[ -z $(which python3) ]]; then
    echo "Error, python3 is not installed or is not in the PATH."
    exit 1
else
    updated_pip=0
    if ! $(echo "import absl"$'\n' | python3 - 2> /dev/null); then
      echo "Installing python packages."
      set -x
      # On Windows bash shell, sudo doesn't exist
      if [[ $(uname) == "Linux" ]] || [[ $(uname) == "Darwin" ]]; then
        sudo python3 -m pip install --upgrade pip
      else
        python3 -m pip install  --upgrade pip
      fi
      pip3 install absl-py
      set +x
    fi
fi

if [[ -z "${ANDROID_HOME}" ]]; then
    echo "Error, ANDROID_HOME environment variable is not set."
    exit 1
fi

if [[ -z "${NDK_ROOT}" || -z $(grep "Pkg\.Revision = 23\." "${NDK_ROOT}/source.properties") ]]; then
    if [[ -d /tmp/android-ndk-r23c && \
              -n $(grep "Pkg\.Revision = 23\." "/tmp/android-ndk-r23c/source.properties") ]]; then
            echo "Using NDK r23c in /tmp/android-ndk-r23c".
    else
        echo "NDK_ROOT environment variable is not set, or NDK version is incorrect."
        echo "This build recommends NDK r23c, downloading..."
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
                    "https://dl.google.com/android/repository/android-ndk-r23c-${platform}.zip" \
                    --output /tmp/android-ndk-r23c.zip && break
                sleep 300
            done
            set -e
            (cd /tmp && unzip -oq android-ndk-r23c.zip && rm -f android-ndk-r23c.zip)
            echo "NDK r23c has been downloaded into /tmp/android-ndk-r23c"
    fi
fi
