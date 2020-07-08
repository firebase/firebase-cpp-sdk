#!/bin/bash

if [[ ! -d testing/test_framework ]]; then
    echo "This script must be run from the firebase-cpp-sdk base directory."
    exit 1
fi

if [[ -d "$1" ]]; then
    cp -a "testing/sample_framework/src" "$1/"
    cp -a "testing/test_framework/src" "$1/"
    cp -a "testing/test_framework/download_googletest.py" "$1/"
else
    # Copy into all directories.
    for dir in admob analytics app auth database dynamic_links firestore functions instance_id messaging remote_config storage
    do
	if [[ -d "${dir}" ]]; then
	    mkdir -p "${dir}/integration_test"
	    # -a preserves file attributes and copies directories recursively.
	    cp -a "testing/sample_framework/src" "${dir}/integration_test/"
	    cp -a "testing/test_framework/src" "${dir}/integration_test/"
	    cp -a "testing/test_framework/download_googletest.py" "${dir}/integration_test/"
	fi
    done
fi
