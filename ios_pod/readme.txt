firebase-cpp-sdk/ios_pod directory

This directory contains a Podfile that allows us to install the Firebase iOS SDK
via CocoaPods. The Podfile here must include all Cocoapods used by the Firebase
C++ SDK. The pods are extracted as part of the build process, and the header
files within them are referenced by the Objective-C++ code in this SDK (see
setup_pod_headers in CMakeLists.txt for details).

The "swift_headers" subdirectory contains copies of the most recent Swift
bridging headers for the Firebase iOS libraries that are written in Swift,
enabling them to be called from this SDK's Objective-C++ code. These headers are
copied from the most recent firebase-ios-sdk release via this repository's
update-dependencies.yml GitHub Actions workflow.
