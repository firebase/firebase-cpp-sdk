# Copyright 2018 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Locate gcc
execute_process(COMMAND /usr/bin/xcrun -sdk iphoneos -find clang
                OUTPUT_VARIABLE CMAKE_C_COMPILER
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Locate g++
execute_process(COMMAND /usr/bin/xcrun -sdk iphoneos -find clang++
                OUTPUT_VARIABLE CMAKE_CXX_COMPILER
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Set the CMAKE_OSX_SYSROOT to the latest SDK found
execute_process(COMMAND /usr/bin/xcrun -sdk iphoneos --show-sdk-path
                OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
                OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "gcc found at: ${CMAKE_C_COMPILER}")
message(STATUS "g++ found at: ${CMAKE_CXX_COMPILER}")
message(STATUS "Using iOS SDK: ${CMAKE_OSX_SYSROOT}")

set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "")
set(CMAKE_XCODE_EFFECTIVE_PLATFORMS "-iphoneos")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE "YES")

# skip TRY_COMPILE checks
set(CMAKE_CXX_COMPILER_WORKS TRUE)
set(CMAKE_C_COMPILER_WORKS TRUE)

set(FIREBASE_IOS_BUILD ON CACHE BOOL "")
set(IOS ON CACHE BOOL "")
