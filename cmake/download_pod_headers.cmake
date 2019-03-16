# Copyright 2018 Google
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Defines the "download_pod_headers" build target, which when built will
# create a subproject that contains the specified Cocoapods. Note that the
# subproject will not be in a buildable state, but the headers will be
# accessible, under ${POD_DIR}/Pods/Headers/Public/*
#
# Args:
#   TARGET_NAME: The name to use for the generated custom target.
#   POD_DIR: The base directory the header files will be stored in.
#            Ideally should be a subdirectory not used for anything else.
#   POD_LIST: The Cocoapods that will be installed.
#             Should be of the format "'Podname', 'Version'" (version optional)
#             These lines are added directly in the pod file, after a "pod"
function(setup_pod_headers_target TARGET_NAME POD_DIR POD_LIST)
  # Guarantee the working directory exists
  file(MAKE_DIRECTORY "${POD_DIR}")

  # Generate the Podfile
  set(podfile "${POD_DIR}/Podfile")
  set(pod_lines "")
  foreach(pod ${POD_LIST})
    string(CONCAT pod_lines "${pod_lines}"
           "  pod ${pod}\\n")
  endforeach(pod)
  string(CONCAT podfile_contents
        "source 'https://github.com/CocoaPods/Specs.git'\\n"
        "platform :ios, '8.0'\\n"
        "target 'GetPods' do\\n"
        "${pod_lines}"
        "end")
  add_custom_command(
    OUTPUT ${podfile}
    COMMAND echo -e "${podfile_contents}" > ${podfile}
    VERBATIM
    # Add a dependency on the CMakeLists file, so that any changes force a
    # rebuild of this process.
    DEPENDS ${CMAKE_CURRENT_LIST_FILE}
  )

  # Create the CMake file needed for the xcode project.
  # This can be done at configure time since the files will never change.
  set(sub_cmake_file "${POD_DIR}/CMakeLists.txt")
  file(WRITE "${sub_cmake_file}"
        "cmake_minimum_required (VERSION 3.1)\n"
        "project (GetPods CXX)\n"
        "add_library(GetPods empty.cc)")
  # Generate the empty source file that the project needs.
  file(WRITE "${POD_DIR}/empty.cc")

  # We use "env -i" to clear the environment variables, and manually keep
  # the PATH to regular bash path.
  # If we didn't do this, we'd end up building for iOS instead of OSX,
  # since the default Xcode environment variables target iOS when you're in
  # an iOS project.
  set(firebase_command_line_path "$ENV{PATH}")
  add_custom_command(
    # Depend on the lock file, as depending on directories can report false
    # positives.
    OUTPUT "${POD_DIR}/Podfile.lock"
    COMMAND env -i PATH=${firebase_command_line_path} cmake . -G Xcode &&
            pod install
    DEPENDS ${podfile}
    WORKING_DIRECTORY ${POD_DIR}
  )

  # Add a custom target that can be depended upon by the library that needs
  # the header files.
  add_custom_target(
    ${TARGET_NAME}
    DEPENDS ${POD_DIR}/Podfile.lock
  )

endfunction()
