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

# Discover all binary framework header paths once after package resolution.
file(GLOB_RECURSE _firebase_spm_all_headers "${FIREBASE_SPM_ARTIFACTS_DIR}/*.h")
set(FIREBASE_SPM_ALL_ARTIFACT_HEADERS "${_firebase_spm_all_headers}" CACHE INTERNAL "Cached SwiftPM artifact header paths")

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
        "${FIREBASE_SPM_CHECKOUTS_DIR}/${module}"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/${module}/spm_headers"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/interop-ios-for-google-sdks/${module}/Public"
        "${FIREBASE_SPM_CHECKOUTS_DIR}/interop-ios-for-google-sdks/${module}/Public/${module}"
      )

      if(module STREQUAL "FirebaseFirestore" OR module STREQUAL "FirebaseFirestoreInternal")
        list(APPEND public_dirs
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/Firestore/core/include"
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/Firestore/Source/Public"
          "${FIREBASE_SPM_CHECKOUTS_DIR}/firebase-ios-sdk/Firestore/Source/Public/FirebaseFirestore"
        )
      endif()

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
      symlink_spm_headers(${target_name} ${module})
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
#   DIR_NAME: Optional custom directory name for the symlink (defaults to matched framework name)
#
function(symlink_spm_headers target_name module_name)
  set(single DIR_NAME)
  cmake_parse_arguments(SYMLINK_SPM "" "${single}" "" ${ARGN})

  set(dir_name "${SYMLINK_SPM_DIR_NAME}")

  set(symlink_base_dir "${CMAKE_CURRENT_BINARY_DIR}/spm_sym_links")

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

  # Build list of exact framework names to match (using IN_LIST to avoid regex metacharacter issues).
  set(patterns "${module_name}")
  if(NOT "${dir_name}" STREQUAL "")
    list(APPEND patterns "${dir_name}")
  endif()
  if("${module_name}" STREQUAL "abseil" OR "${module_name}" STREQUAL "absl")
    list(APPEND patterns "absl")
  elseif("${module_name}" STREQUAL "grpc" OR
         "${module_name}" STREQUAL "gRPC" OR
         "${module_name}" STREQUAL "gRPC-C++" OR
         "${module_name}" STREQUAL "gRPC-Core" OR
         "${module_name}" STREQUAL "grpcpp")
    list(APPEND patterns "grpc" "grpcpp")
  elseif("${module_name}" STREQUAL "GoogleUserMessagingPlatform" OR
         "${module_name}" STREQUAL "UserMessagingPlatform")
    list(APPEND patterns "UserMessagingPlatform")
  endif()

  set(processed_frameworks "")

  # Pass 1: Prefer headers matching preferred_slice; Pass 2: Fallback to any slice.
  foreach(require_preferred TRUE FALSE)
    foreach(hdr IN LISTS FIREBASE_SPM_ALL_ARTIFACT_HEADERS)
      # Extract both the full .../Foo.framework/Headers path and Foo in a single regex.
      if(hdr MATCHES "^(.*/([^/\\\\]+)\\.framework([^/\\\\]*)?/Headers)/")
        set(fw_headers "${CMAKE_MATCH_1}")
        set(fw_name "${CMAKE_MATCH_2}")

        if(NOT "${fw_name}" IN_LIST processed_frameworks AND "${fw_name}" IN_LIST patterns)
          if((require_preferred AND fw_headers MATCHES "${preferred_slice}") OR NOT require_preferred)
            file(MAKE_DIRECTORY "${symlink_base_dir}")
            set(link_name "${fw_name}")
            if(NOT "${dir_name}" STREQUAL "")
              set(link_name "${dir_name}")
            endif()
            if(EXISTS "${symlink_base_dir}/${link_name}" OR IS_SYMLINK "${symlink_base_dir}/${link_name}")
              file(REMOVE_RECURSE "${symlink_base_dir}/${link_name}")
            endif()
            file(CREATE_LINK "${fw_headers}" "${symlink_base_dir}/${link_name}" SYMBOLIC COPY_ON_ERROR)

            target_include_directories(${target_name}
              PUBLIC
                "${symlink_base_dir}"
                "${symlink_base_dir}/${link_name}"
                "${fw_headers}"
            )
            list(APPEND processed_frameworks "${fw_name}")
          endif()
        endif()
      endif()
    endforeach()
  endforeach()
endfunction()
