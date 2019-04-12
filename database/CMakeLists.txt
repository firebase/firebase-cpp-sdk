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

# CMake file for the firebase_database library

cmake_minimum_required (VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

include(binary_to_array)
include(download_pod_headers)

project(firebase_database NONE)
enable_language(C)
enable_language(CXX)

# Common source files used by all platforms
set(common_SRCS
    src/common/common.cc
    src/common/data_snapshot.cc
    src/common/database.cc
    src/common/database_reference.cc
    src/common/disconnection.cc
    src/common/listener.cc
    src/common/mutable_data.cc
    src/common/query.cc)

# Define the resource build needed for Android
binary_to_array("database_resources"
    "${CMAKE_CURRENT_LIST_DIR}/database_resources/build/dexed.jar"
    "firebase_database_resources"
    "${FIREBASE_GEN_FILE_DIR}/database")

# Source files used by the Android implementation.
set(android_SRCS
    ${database_resources_source}
    src/android/data_snapshot_android.cc
    src/android/database_android.cc
    src/android/database_reference_android.cc
    src/android/disconnection_android.cc
    src/android/mutable_data_android.cc
    src/android/query_android.cc
    src/android/util_android.cc)

# Source files used by the iOS implementation.
set(ios_SRCS
    src/ios/completion_block_ios.mm
    src/ios/data_snapshot_ios.mm
    src/ios/database_ios.mm
    src/ios/database_reference_ios.mm
    src/ios/disconnection_ios.mm
    src/ios/mutable_data_ios.mm
    src/ios/query_ios.mm
    src/ios/util_ios.mm)

if (NOT OPENSSL_FOUND)
  # Only log a message if building for Desktop, since mobile doesn't care.
  if (NOT ANDROID AND NOT IOS)
    message(FATAL_ERROR "Unable to find OpenSSL, which is needed for Realtime \
            Database's desktop implementation. Install OpenSSL and add it to \
            your PATH, or set the CMake option FIREBASE_INCLUDE_DATABASE to \
            OFF to disable the firebase_database build target.")
  endif()
endif()

# Source files used by the desktop implementation.
set(desktop_SRCS
    src/desktop/connection/connection.cc
    src/desktop/connection/host_info.cc
    src/desktop/connection/persistent_connection.cc
    src/desktop/connection/util_connection.cc
    src/desktop/connection/web_socket_client_impl.cc
    src/desktop/core/child_event_registration.cc
    src/desktop/core/compound_write.cc
    src/desktop/core/event_registration.cc
    src/desktop/core/indexed_variant.cc
    src/desktop/core/keep_synced_event_registration.cc
    src/desktop/core/listen_provider.cc
    src/desktop/core/operation.cc
    src/desktop/core/repo.cc
    src/desktop/core/server_values.cc
    src/desktop/core/sparse_snapshot_tree.cc
    src/desktop/core/sync_point.cc
    src/desktop/core/sync_tree.cc
    src/desktop/core/tracked_query_manager.cc
    src/desktop/core/value_event_registration.cc
    src/desktop/core/web_socket_listen_provider.cc
    src/desktop/core/write_tree.cc
    src/desktop/data_snapshot_desktop.cc
    src/desktop/database_desktop.cc
    src/desktop/database_reference_desktop.cc
    src/desktop/disconnection_desktop.cc
    src/desktop/mutable_data_desktop.cc
    src/desktop/persistence/in_memory_persistence_storage_engine.cc
    src/desktop/persistence/persistence_manager.cc
    src/desktop/push_child_name_generator.cc
    src/desktop/query_desktop.cc
    src/desktop/query_params_comparator.cc
    src/desktop/transaction_data.cc
    src/desktop/util_desktop.cc
    src/desktop/view/change.cc
    src/desktop/view/child_change_accumulator.cc
    src/desktop/view/event_generator.cc
    src/desktop/view/indexed_filter.cc
    src/desktop/view/limited_filter.cc
    src/desktop/view/ranged_filter.cc
    src/desktop/view/view.cc
    src/desktop/view/view_processor.cc)

if(ANDROID)
  set(database_platform_SRCS
      "${android_SRCS}")
elseif(IOS)
  set(database_platform_SRCS
      "${ios_SRCS}")
else()
  set(database_platform_SRCS
      "${desktop_SRCS}")
endif()

if(ANDROID OR IOS)
  set(additional_include_DIR)
  set(additional_link_LIB)
  set(additional_DEFINES)
else()
  set(additional_include_DIR
      ${flatbuffers_SOURCE_DIR}/include
      ${OPENSSL_INCLUDE_DIR}
      ${uWebSockets_SOURCE_DIR}/..)

  set(additional_link_LIB
      flatbuffers
      firebase_rest_lib
      ${OPENSSL_CRYPTO_LIBRARY}
      libuWS)

  set(additional_DEFINES
      -DFIREBASE_TARGET_DESKTOP=1)
endif()

if(MSVC)
  set(additional_DEFINES
    "${additional_DEFINES}"
    -DWIN32_LEAN_AND_MEAN # Ensure that windows doesn't include winsock.h by
                            # default, as it can cause issues when libraries try
                            # to include winsock2.h later on in the process.
  )
endif()

add_library(firebase_database STATIC
    ${common_SRCS}
    ${database_platform_SRCS})

# Set up the dependency on Firebase App.
target_link_libraries(firebase_database
  PUBLIC
    firebase_app
  PRIVATE
    ${additional_link_LIB}
)
# Public headers all refer to each other relative to the src/include directory,
# while private headers are relative to the entire C++ SDK directory.
target_include_directories(firebase_database
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${additional_include_DIR}
)
target_compile_definitions(firebase_database
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
    ${additional_DEFINES}
)
# Automatically include headers that might not be declared.
if(MSVC)
  add_definitions(/FI"assert.h" /FI"string.h" /FI"stdint.h")
else()
  add_definitions(-include assert.h -include string.h)
endif()

if(IOS)
  # Enable Automatic Reference Counting (ARC)
  set_property(
    TARGET firebase_database
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  # Setup the target for getting the Firebase iOS SDK headers through Cocoapods.
  set(pod_target_name "download_database_pod_headers")
  set(pods_dir "${PROJECT_BINARY_DIR}/Pods")
  set(pod_list "")
  list(APPEND pod_list "'Firebase/Core', '5.20.2'")
  list(APPEND pod_list "'Firebase/Database', '5.20.2'")

  setup_pod_headers_target("${pod_target_name}" "${pods_dir}" "${pod_list}")

  # Add the Cocoapod headers to the include directories
  set(base_header_dir "${pods_dir}/Pods/Headers/Public")
  target_include_directories(firebase_database
    PRIVATE
      ${base_header_dir}/FirebaseCore
      ${base_header_dir}/FirebaseDatabase
  )

  # Add a dependency to downloading the headers onto database.
  add_dependencies(firebase_database ${pod_target_name})
endif()
