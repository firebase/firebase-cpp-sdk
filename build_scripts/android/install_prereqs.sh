#!/bin/bash -e

if [[ $(uname) =~ "Darwin" ]]; then
    platform=darwin
elif [[ $(uname) =~ "Linux" ]]; then
    platform=linux
else
    platform=windows
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
	curl -LSs \
	     "https://dl.google.com/android/repository/android-ndk-r16b-${platform}-x86_64.zip" \
	     --output /tmp/android-ndk-r16b.zip
	(cd /tmp && unzip -q android-ndk-r16b.zip && rm -f android-ndk-r16b.zip)
	echo "NDK r16b has been downloaded into /tmp/android-ndk-r16b"
    fi
fi
