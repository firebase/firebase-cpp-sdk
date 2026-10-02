# firebase-cpp-sdk/ios_spm directory

This directory contains a `Package.swift` file that allows us to fetch the
Firebase iOS SDK and Google User Messaging Platform via Swift Package Manager
(SwiftPM). The `Package.swift` here must declare all Swift packages used by
the Firebase C++ SDK. The packages are resolved automatically during the CMake
configure step, and the header files and xcframeworks within them are
referenced by the Objective-C++ and C++ code in this SDK (see `setup_spm_headers`
in `CMakeLists.txt` for details).

The `swift_headers` subdirectory contains copies of the most recent Swift
bridging headers (`*-Swift.h`) for the Firebase iOS libraries that are written
in Swift, enabling them to be called from this SDK's Objective-C++ code. These
headers are updated via this repository's `update-dependencies.yml` GitHub
Actions workflow.
