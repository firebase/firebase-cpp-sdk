# Copyright 2019 Google
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

if(NOT IOS)
  return()
endif()

# Add these two vars to the cache to make them avaliable in global scope
set(FIREBASE_POD_DIR ${CMAKE_CURRENT_BINARY_DIR} CACHE STRING "")
set(FIREBASE_POD_HEADER_DIR
  "${FIREBASE_POD_DIR}/Pods/Headers/Public"
  CACHE STRING ""
)

# Copy build files to trigger pod install as it requires a valid Xcode project
file(
  COPY
    Podfile
    empty.cc
    empty_CMakeLists.txt DESTINATION
  "${FIREBASE_POD_DIR}"
)

# Rename cmake file
file(
  RENAME
    "${FIREBASE_POD_DIR}/empty_CMakeLists.txt"
    "${FIREBASE_POD_DIR}/CMakeLists.txt"
)

message("Downloading CocoaPods...")

execute_process(
  COMMAND /bin/bash -c
          "mkdir -p ${FIREBASE_POD_DIR} && cd ${FIREBASE_POD_DIR} && cmake . -G Xcode && pod install"
  OUTPUT_VARIABLE pods_output
)

message("${pods_output}")



# Sets up a target to have the correct dependencies on the install pods target
# and also adds the correct include paths to the pod header files
#
# Usage: setup_pod_headers(<target_name>
#                          POD_NAMES [<pod_names>...]
#                         )
#
# Args:
#   target_name: Name of target to add includes to
#   pod_names: Array of pod names to configure
#
function(setup_pod_headers target_name)
  set(multi OUTPUT_NAME POD_NAMES)

  # Parse the arguments into SETUP_POD_HEADERS_POD_NAMES.
  cmake_parse_arguments(SETUP_POD_HEADERS "" "" "${multi}" ${ARGN})

  if(NOT "${SETUP_POD_HEADERS_POD_NAMES}" STREQUAL "")
    foreach (pod ${SETUP_POD_HEADERS_POD_NAMES})
      # Add pod name twice as some frameworks moved includes inside a folder with
      # the pod name
      target_include_directories(${target_name}
        PRIVATE
          ${FIREBASE_POD_HEADER_DIR}/${pod}
          ${FIREBASE_POD_HEADER_DIR}/${pod}/${pod}
      )

      symlink_pod_headers(${target_name} ${pod} ${pod})
    endforeach()
  endif()
endfunction()

# Creates a symlink to the header files of the given framework. Used when
# include paths are expecting the header files to be in a subdirectory, when
# accessing the header files directly does not have them in the same structure.
#
# Usage: symlink_pod_headers(<target_name> <pod_name> <dir_name>
#                             [FRAMEWORK_DIR_NAME <framework_dir>]
#                           )
#
# Args:
#   target_name: Name of target to add includes to
#   pod_name: Name of the pod in the Podfile
#   dir_name: Name of the dir for the symlink (most cases should be same as pod_name)
#   framework_dir: Set if pod uses a different folder name for frameworks
#
function(symlink_pod_headers target_name pod_name dir_name)

  set(multi OUTPUT_NAME FRAMEWORK_DIR_NAME)

  # Parse the arguments into SYMLINK_POD_FRAMEWORK_DIR_NAME.
  cmake_parse_arguments(SYMLINK_POD "" "" "${multi}" ${ARGN})

  set(FRAMEWORK_DIR "Frameworks")

  if(NOT "${SYMLINK_POD_FRAMEWORK_DIR_NAME}" STREQUAL "")
    set(FRAMEWORK_DIR "${SYMLINK_POD_FRAMEWORK_DIR_NAME}")
  endif()

  set(gen_file ${CMAKE_CURRENT_BINARY_DIR}/pod_sym_links/gen_${dir_name}.txt)
  set(symlink_dir ${CMAKE_CURRENT_BINARY_DIR}/pod_sym_links/${dir_name})
  set(glob_dir "${FIREBASE_POD_DIR}/Pods/${pod_name}/${FRAMEWORK_DIR}/*/*.framework/Headers")

  file(GLOB framework_dir ${glob_dir})

  # If failed to find headers, try again one folder up
  if(NOT EXISTS ${framework_dir})
    set(glob_dir "${FIREBASE_POD_DIR}/Pods/${pod_name}/${FRAMEWORK_DIR}/*.framework/Headers")
    file(GLOB framework_dir ${glob_dir})
  endif()

  # Maybe this pod is not a framework, continue
  if(NOT EXISTS ${framework_dir})
    return()
  endif()

  add_custom_command(
    OUTPUT "${gen_file}"
    COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/pod_sym_links
            && rm -fr ${symlink_dir}
            && ln -sf ${framework_dir} ${symlink_dir}
            && touch ${gen_file}
    DEPENDS ${FIREBASE_POD_HEADER_DIR}
    COMMENT "Sym linking pod framework '${framework_dir}' to '${symlink_dir}'"
  )

  add_custom_target(${target_name}_symlink_${dir_name} DEPENDS ${gen_file})
  add_dependencies(${target_name} ${target_name}_symlink_${dir_name})

  target_include_directories(${target_name}
    PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/pod_sym_links
      ${CMAKE_CURRENT_BINARY_DIR}/pod_sym_links/${dir_name}
  )
endfunction()
