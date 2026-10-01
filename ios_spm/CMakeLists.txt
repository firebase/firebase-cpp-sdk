# Copyright 2026 Google LLC
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

# Global variables for Swift Package Manager paths
set(FIREBASE_SPM_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE STRING "Directory for Swift Package Manager resolution" FORCE)
set(FIREBASE_SPM_CHECKOUTS_DIR "${FIREBASE_SPM_DIR}/.build/checkouts" CACHE STRING "Path to SwiftPM package checkouts" FORCE)
set(FIREBASE_SPM_ARTIFACTS_DIR "${FIREBASE_SPM_DIR}/.build/artifacts" CACHE STRING "Path to SwiftPM binary artifacts" FORCE)
set(FIREBASE_SPM_SWIFT_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/swift_headers" CACHE STRING "Path to Swift bridging headers" FORCE)

# Copy Package.swift to the build directory for resolution
file(COPY "${CMAKE_CURRENT_LIST_DIR}/Package.swift" DESTINATION "${FIREBASE_SPM_DIR}")

message(STATUS "Resolving Swift Package dependencies for iOS/tvOS...")

execute_process(
  COMMAND ${CMAKE_COMMAND} -E env
          GIT_CONFIG_COUNT=1
          GIT_CONFIG_KEY_0=safe.bareRepository
          GIT_CONFIG_VALUE_0=all
          swift package --package-path "${FIREBASE_SPM_DIR}" resolve
  RESULT_VARIABLE spm_result
  OUTPUT_VARIABLE spm_output
  ERROR_VARIABLE spm_error
)

if(NOT spm_result EQUAL 0)
  message(FATAL_ERROR "Failed to resolve Swift packages via SwiftPM:\n${spm_error}\n${spm_output}")
endif()

message(STATUS "Swift Package dependencies resolved successfully.")

# Sets up a target with the include directories for the specified SwiftPM modules.
#
# Usage: setup_spm_headers(<target_name>
#                          MODULES [<module_names>...]
#                          [INCLUDE_PRIVATE_HEADERS]
#                         )
#
# Args:
#   target_name: Name of target to add includes to
#   MODULES: Array of module names to configure (e.g. FirebaseCore, FirebaseAuth, etc.)
#   INCLUDE_PRIVATE_HEADERS: Optional flag to also include internal / extension headers
#
function(setup_spm_headers target_name)
  set(multi MODULES)
  set(options INCLUDE_PRIVATE_HEADERS)

  cmake_parse_arguments(SETUP_SPM "${options}" "" "${multi}" ${ARGN})

  if(NOT "${SETUP_SPM_MODULES}" STREQUAL "")
    foreach(module ${SETUP_SPM_MODULES})
      # Check source checkouts under firebase-ios-sdk and root checkouts
      set(public_dirs
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Sources/Public"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Sources/Public/${module}"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Source/Library/Public"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Source/Library/Public/${module}"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Interop/Public"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Interop/Public/${module}"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Public"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/Firestore/core/include"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/Firestore/Source/Public"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/Firestore/Source/Public/FirebaseFirestore"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/${module}"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/${module}/spm_headers"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/interop-ios-for-google-sdks/${module}/Public"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/interop-ios-for-google-sdks/${module}/Public/${module}"
      )

      foreach(pdir ${public_dirs})
        if(EXISTS "${pdir}")
          target_include_directories(${target_name} PUBLIC "${pdir}")
        endif()
      endforeach()

      if(SETUP_SPM_INCLUDE_PRIVATE_HEADERS)
        set(private_dirs
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Extension"
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Sources"
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Sources/Private"
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Source"
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/Source/Library/Private"
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/${module}/include"
        )
        foreach(pdir ${private_dirs})
          if(EXISTS "${pdir}")
            target_include_directories(${target_name} PUBLIC "${pdir}")
          endif()
        endforeach()
      endif()

      # Also attempt to symlink any binary xcframework headers for this module
      symlink_spm_headers(${target_name} ${module} DIR_NAME ${module})
    endforeach()
  endif()
endfunction()

# Creates a symlink to the header files of the given binary xcframework.
# Used when include paths are expecting headers to be in a subdirectory or framework structure.
#
# Usage: symlink_spm_headers(<target_name> <module_name>
#                            [DIR_NAME <dir_name>]
#                           )
#
# Args:
#   target_name: Name of target to add includes to
#   module_name: Name of the binary target / artifact
#   DIR_NAME: Name of the directory for the symlink (defaults to module_name)
#
function(symlink_spm_headers target_name module_name)
  set(single DIR_NAME)
  cmake_parse_arguments(SYMLINK_SPM "" "${single}" "" ${ARGN})

  set(dir_name "${module_name}")
  if(NOT "${SYMLINK_SPM_DIR_NAME}" STREQUAL "")
    set(dir_name "${SYMLINK_SPM_DIR_NAME}")
  endif()

  set(symlink_base_dir "${CMAKE_CURRENT_BINARY_DIR}/spm_sym_links")

  # Search for all header files in .build/artifacts to discover binary frameworks
  file(GLOB_RECURSE all_headers "${FIREBASE_SPM_ARTIFACTS_DIR}/*.h")

  # Determine target slice priority if possible
  string(TOLOWER "${CMAKE_OSX_SYSROOT}" sysroot_lower)
  if(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    if(sysroot_lower MATCHES "simulator")
      set(preferred_slice "tvos-arm64_x86_64-simulator")
    else()
      set(preferred_slice "tvos-arm64")
    endif()
  else()
    if(sysroot_lower MATCHES "simulator")
      set(preferred_slice "ios-arm64_x86_64-simulator")
    else()
      set(preferred_slice "ios-arm64")
    endif()
  endif()

  # Build list of patterns to match
  set(patterns "${module_name}" "${dir_name}")
  if(module_name MATCHES "^(abseil|absl)$")
    list(APPEND patterns "absl")
  elseif(module_name MATCHES "^(grpc|gRPC|gRPC-C\\+\\+|gRPC-Core|grpcpp)$")
    list(APPEND patterns "grpc" "grpcpp")
  elseif(module_name MATCHES "^(GoogleUserMessagingPlatform|UserMessagingPlatform)$")
    list(APPEND patterns "UserMessagingPlatform")
  endif()

  set(processed_frameworks "")

  foreach(hdr ${all_headers})
    if(hdr MATCHES "([^/\\\\]+)\\.framework.*Headers")
      set(fw_name "${CMAKE_MATCH_1}")
      string(REGEX MATCH "(.*[/\\\\]${fw_name}\\.framework([^/\\\\]*)?[/\\\\]Headers)" _ "${hdr}")
      set(fw_headers "${CMAKE_MATCH_1}")

      if(NOT "${fw_name}" IN_LIST processed_frameworks)
        set(matches FALSE)
        foreach(pat ${patterns})
          if(fw_name MATCHES "^${pat}$")
            set(matches TRUE)
            break()
          endif()
        endforeach()

        if(matches)
          # Check if this header matches preferred slice or if we don't have one yet
          if(fw_headers MATCHES "${preferred_slice}" OR NOT EXISTS "${symlink_base_dir}/${fw_name}")
            file(MAKE_DIRECTORY "${symlink_base_dir}")
            if(EXISTS "${symlink_base_dir}/${fw_name}")
              file(REMOVE "${symlink_base_dir}/${fw_name}")
            endif()
            file(CREATE_LINK "${fw_headers}" "${symlink_base_dir}/${fw_name}" SYMBOLIC COPY_ON_ERROR)

            target_include_directories(${target_name}
              PUBLIC
                "${symlink_base_dir}"
                "${symlink_base_dir}/${fw_name}"
                "${fw_headers}"
            )
            if(fw_headers MATCHES "${preferred_slice}")
              list(APPEND processed_frameworks "${fw_name}")
            endif()
          endif()
        endif()
      endif()
    endif()
  endforeach()
endfunction()
