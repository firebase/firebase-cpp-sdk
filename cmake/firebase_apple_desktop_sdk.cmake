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

# Embeds the Firebase Apple Analytics SDK into macOS desktop binaries.
#
# firebase_analytics on macOS desktop contains a backend
# (analytics/src/analytics_desktop_apple.mm) that forwards to FIRAnalytics when
# the Firebase Apple SDK is present in the process. This module provides the
# function that puts it there:
#
#   firebase_cpp_link_apple_analytics_sdk(<target>
#                                         [SDK_DIR <dir>]
#                                         [KEEP_SDK_SYMBOLS_EXPORTED])
#
# It
#  * builds the SDK with analytics/apple_desktop/build_apple_analytics_sdk.py
#    (once per build tree, cached by version / archs / deployment target), or
#    uses a prebuilt one from SDK_DIR / FIREBASE_APPLE_ANALYTICS_SDK_DIR,
#  * force-loads the SDK archives (like Xcode does for SwiftPM packages, so
#    ObjC categories and +load registrations are kept),
#  * de-duplicates nanopb against desktop Firestore's copy,
#  * for shared libraries / bundles, hides the SDK's symbols so they can't
#    collide with or interpose on other images and can be stripped,
#  * runs scripts/check_duplicate_symbols.py before linking, comparing the SDK
#    against every static library <target> links (call this function after
#    target_link_libraries()),
#  * links Swift runtime search paths and the system frameworks the SDK needs.
#
# Size: -dead_strip (enabled below for non-Debug builds) removes unreferenced
# code; hidden symbols let `strip -x` remove their names as well. The per-module
# size report is written to <sdk dir>/size_report.txt.

if(NOT APPLE OR IOS)
  return()
endif()

set(FIREBASE_APPLE_ANALYTICS_SDK_DIR "" CACHE PATH
    "Prebuilt Firebase Apple Analytics SDK (output of build_apple_analytics_sdk.py). Built on demand if empty.")
option(FIREBASE_APPLE_ANALYTICS_SDK_CHECK_DUPLICATE_SYMBOLS
       "Fail the link if the embedded Firebase Apple SDK duplicates symbols of other static libraries." ON)

set(_FIREBASE_APPLE_SDK_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_FIREBASE_APPLE_SDK_REPO_DIR
                       "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Collects the static libraries a target links, transitively, as
# $<TARGET_FILE:...> expressions. Only libraries known when this runs are
# found, so call firebase_cpp_link_apple_analytics_sdk() after
# target_link_libraries().
function(_firebase_cpp_collect_static_libraries OUT_VAR)
  set(pending ${ARGN})
  set(seen)
  set(result)
  while(pending)
    list(POP_FRONT pending item)
    if(item MATCHES "^\\$<LINK_ONLY:(.+)>$")
      set(item "${CMAKE_MATCH_1}")
    endif()
    if(NOT TARGET "${item}" OR "${item}" IN_LIST seen)
      continue()
    endif()
    list(APPEND seen "${item}")
    get_target_property(aliased "${item}" ALIASED_TARGET)
    if(aliased)
      set(item "${aliased}")
    endif()
    get_target_property(type "${item}" TYPE)
    if(type STREQUAL "STATIC_LIBRARY")
      list(APPEND result "$<TARGET_FILE:${item}>")
    endif()
    foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
      get_target_property(dependencies "${item}" ${property})
      if(dependencies)
        list(APPEND pending ${dependencies})
      endif()
    endforeach()
  endwhile()
  list(REMOVE_DUPLICATES result)
  set(${OUT_VAR} "${result}" PARENT_SCOPE)
endfunction()

# Builds (or reuses) the SDK and loads its manifest into the caller's scope.
function(_firebase_cpp_load_apple_analytics_sdk SDK_DIR_VAR)
  set(sdk_dir "${${SDK_DIR_VAR}}")
  if(sdk_dir STREQUAL "")
    set(sdk_dir "${CMAKE_BINARY_DIR}/firebase_apple_analytics_sdk")
    if(CMAKE_OSX_ARCHITECTURES)
      set(archs ${CMAKE_OSX_ARCHITECTURES})
    else()
      set(archs ${CMAKE_HOST_SYSTEM_PROCESSOR})
    endif()
    # CMake reports Apple Silicon hosts as arm64 and Intel as x86_64, which
    # match SwiftPM's --arch names.
    if(NOT FIREBASE_PYTHON_EXECUTABLE)
      find_package(Python3 COMPONENTS Interpreter REQUIRED)
      set(FIREBASE_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
    endif()
    message(STATUS "Building the Firebase Apple Analytics SDK for macOS (${archs}) in ${sdk_dir}")
    execute_process(
      COMMAND "${FIREBASE_PYTHON_EXECUTABLE}"
              "${_FIREBASE_APPLE_SDK_REPO_DIR}/analytics/apple_desktop/build_apple_analytics_sdk.py"
              --output-dir "${sdk_dir}"
              --archs ${archs}
              --deployment-target "${CMAKE_OSX_DEPLOYMENT_TARGET}"
      RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
      message(FATAL_ERROR
        "Failed to build the Firebase Apple Analytics SDK for macOS. Set "
        "FIREBASE_APPLE_ANALYTICS_SDK_DIR to a prebuilt copy, or disable "
        "embedding it.")
    endif()
  endif()
  set(manifest "${sdk_dir}/firebase_apple_analytics_sdk.cmake")
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "${manifest} not found; is ${sdk_dir} a Firebase Apple Analytics SDK build?")
  endif()
  include("${manifest}")
  foreach(var VERSION ARCHS DEPLOYMENT_TARGET MODULES ARCHIVES HAS_SWIFT
              SWIFT_LIBRARY_DIRS SYSTEM_FRAMEWORKS SYSTEM_LIBRARIES
              PRIVACY_MANIFEST)
    set(FIREBASE_APPLE_ANALYTICS_SDK_${var}
        "${FIREBASE_APPLE_ANALYTICS_SDK_${var}}" PARENT_SCOPE)
  endforeach()
  set(${SDK_DIR_VAR} "${sdk_dir}" PARENT_SCOPE)
endfunction()

function(firebase_cpp_link_apple_analytics_sdk TARGET)
  cmake_parse_arguments(ARG "KEEP_SDK_SYMBOLS_EXPORTED" "SDK_DIR" "" ${ARGN})
  if(NOT TARGET ${TARGET})
    message(FATAL_ERROR "firebase_cpp_link_apple_analytics_sdk: ${TARGET} is not a target.")
  endif()

  set(sdk_dir "${ARG_SDK_DIR}")
  if(sdk_dir STREQUAL "")
    set(sdk_dir "${FIREBASE_APPLE_ANALYTICS_SDK_DIR}")
  endif()
  _firebase_cpp_load_apple_analytics_sdk(sdk_dir)

  if(CMAKE_OSX_DEPLOYMENT_TARGET AND
     CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS FIREBASE_APPLE_ANALYTICS_SDK_DEPLOYMENT_TARGET)
    message(WARNING
      "${TARGET} targets macOS ${CMAKE_OSX_DEPLOYMENT_TARGET}, but the embedded "
      "Firebase Apple SDK requires macOS ${FIREBASE_APPLE_ANALYTICS_SDK_DEPLOYMENT_TARGET}.")
  endif()

  # nanopb: desktop Firestore builds its own copy (protobuf-nanopb-static,
  # nanopb 0.3.9.x with PB_FIELD_32BIT / PB_ENABLE_MALLOC /
  # PB_NO_PACKED_STRUCTS, the same configuration as the SwiftPM package).
  # Embedding both would duplicate pb_* symbols, so reuse Firestore's.
  set(dedupe_nanopb OFF)
  if(TARGET protobuf-nanopb-static)
    set(dedupe_nanopb ON)
  endif()

  set(forced_archives)
  set(embedded_modules)
  list(LENGTH FIREBASE_APPLE_ANALYTICS_SDK_MODULES module_count)
  math(EXPR last_index "${module_count} - 1")
  foreach(index RANGE ${last_index})
    list(GET FIREBASE_APPLE_ANALYTICS_SDK_MODULES ${index} module)
    list(GET FIREBASE_APPLE_ANALYTICS_SDK_ARCHIVES ${index} archive)
    if(dedupe_nanopb AND module MATCHES "^nanopb$")
      continue()
    endif()
    list(APPEND forced_archives "${archive}")
    list(APPEND embedded_modules "${module}")
    target_link_options(${TARGET} PRIVATE "LINKER:-force_load,${archive}")
  endforeach()
  # Libraries are appended to the LINK_LIBRARIES property instead of using
  # target_link_libraries(): CMake rejects mixing its keyword and plain
  # signatures on one target, and callers (e.g. the Unity bundle) use either.
  if(dedupe_nanopb)
    set_property(TARGET ${TARGET} APPEND PROPERTY LINK_LIBRARIES protobuf-nanopb-static)
    message(STATUS "${TARGET}: embedding Firebase Apple SDK with desktop Firestore's nanopb (de-duplicated).")
  endif()
  # Relink when the SDK changes.
  set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS ${forced_archives})

  foreach(framework ${FIREBASE_APPLE_ANALYTICS_SDK_SYSTEM_FRAMEWORKS})
    set_property(TARGET ${TARGET} APPEND PROPERTY LINK_LIBRARIES "-framework ${framework}")
  endforeach()
  foreach(library ${FIREBASE_APPLE_ANALYTICS_SDK_SYSTEM_LIBRARIES})
    set_property(TARGET ${TARGET} APPEND PROPERTY LINK_LIBRARIES "-l${library}")
  endforeach()
  if(FIREBASE_APPLE_ANALYTICS_SDK_HAS_SWIFT)
    # FirebaseCoreInternal is Swift. Its objects auto-link the Swift runtime
    # (swiftCore, swiftFoundation, compatibility shims); point the linker at
    # them. The runtime itself ships with macOS in /usr/lib/swift.
    target_link_directories(${TARGET} PRIVATE ${FIREBASE_APPLE_ANALYTICS_SDK_SWIFT_LIBRARY_DIRS})
    target_link_options(${TARGET} PRIVATE "LINKER:-rpath,/usr/lib/swift")
  endif()

  get_target_property(target_type ${TARGET} TYPE)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(${TARGET} PRIVATE "LINKER:-dead_strip")
  endif()
  if((target_type STREQUAL "SHARED_LIBRARY" OR target_type STREQUAL "MODULE_LIBRARY")
     AND NOT ARG_KEEP_SDK_SYMBOLS_EXPORTED)
    # Hide everything the SDK defines. Nothing outside the image needs it:
    # the Objective-C and Swift runtimes find classes through metadata
    # sections, not exported symbols. Hidden symbols can't be interposed by
    # (or interpose on) another copy of the Firebase Apple SDK loaded in the
    # same process, and become local so `strip -x` removes them.
    target_link_options(${TARGET} PRIVATE
      "LINKER:-unexported_symbols_list,${_FIREBASE_APPLE_SDK_CMAKE_DIR}/firebase_apple_desktop_sdk_unexported_symbols.txt")
  endif()

  if(FIREBASE_APPLE_ANALYTICS_SDK_CHECK_DUPLICATE_SYMBOLS)
    if(NOT FIREBASE_PYTHON_EXECUTABLE)
      find_package(Python3 COMPONENTS Interpreter REQUIRED)
      set(FIREBASE_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
    endif()
    get_target_property(target_link_libraries ${TARGET} LINK_LIBRARIES)
    _firebase_cpp_collect_static_libraries(linked_archives ${target_link_libraries})
    list(GET FIREBASE_APPLE_ANALYTICS_SDK_ARCHS 0 check_arch)
    add_custom_command(TARGET ${TARGET} PRE_LINK
      COMMAND "${FIREBASE_PYTHON_EXECUTABLE}"
              "${_FIREBASE_APPLE_SDK_REPO_DIR}/scripts/check_duplicate_symbols.py"
              --arch ${check_arch}
              --force-load ${forced_archives}
              --link ${linked_archives}
      COMMENT "Checking ${TARGET} for duplicate symbols with the Firebase Apple SDK"
      VERBATIM
    )
  endif()

  set_property(TARGET ${TARGET} PROPERTY FIREBASE_APPLE_ANALYTICS_SDK_PRIVACY_MANIFEST
               "${FIREBASE_APPLE_ANALYTICS_SDK_PRIVACY_MANIFEST}")
  set_property(TARGET ${TARGET} PROPERTY FIREBASE_APPLE_ANALYTICS_SDK_DIR "${sdk_dir}")
  list(JOIN embedded_modules ", " embedded_modules_text)
  message(STATUS "${TARGET}: embedding Firebase Apple SDK ${FIREBASE_APPLE_ANALYTICS_SDK_VERSION} modules: ${embedded_modules_text}")
  if(EXISTS "${sdk_dir}/size_report.txt")
    file(READ "${sdk_dir}/size_report.txt" size_report)
    message(STATUS "${size_report}")
  endif()
endfunction()
