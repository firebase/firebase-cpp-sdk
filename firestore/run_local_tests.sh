#!/bin/bash
# Runs the presubmit tests that must be run locally, because they don't work in
# TAP or Guitar at the moment (see b/135205911). Execute this script before
# submitting.

# LINT.IfChange
blaze test --config=android_arm //firebase/firestore/client/cpp:kokoro_build_test && \
blaze test --config=darwin_x86_64 //firebase/firestore/client/cpp:kokoro_build_test && \
blaze test --define=force_regular_grpc=1 //firebase/firestore/client/cpp:kokoro_build_test && \
blaze test --config=msvc //firebase/firestore/client/cpp:kokoro_build_test
# LINT.ThenChange(//depot_firebase_cpp/firestore/client/cpp/METADATA)
