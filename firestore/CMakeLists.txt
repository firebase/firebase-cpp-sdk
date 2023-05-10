# Copyright 2020 Google LLC
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

if(APPLE AND NOT ANDROID)
  set(settings_apple_SRCS
      src/common/settings_apple.mm)
else()
  set(settings_apple_SRCS)
endif()

set(common_SRCS
    src/common/aggregate_query.cc
    src/common/aggregate_query_snapshot.cc
    src/common/cleanup.h
    src/common/collection_reference.cc
    src/common/compiler_info.cc
    src/common/compiler_info.h
    src/common/document_change.cc
    src/common/document_reference.cc
    src/common/document_snapshot.cc
    src/common/exception_common.cc
    src/common/exception_common.h
    src/common/field_path.cc
    src/common/field_value.cc
    src/common/firestore.cc
    src/common/firestore_exceptions_common.h
    src/common/futures.cc
    src/common/futures.h
    src/common/hard_assert_common.cc
    src/common/hard_assert_common.h
    src/common/listener_registration.cc
    src/common/load_bundle_task_progress.cc
    src/common/macros.h
    src/common/query.cc
    src/common/query_snapshot.cc
    src/common/set_options.cc
    src/common/settings.cc
    src/common/snapshot_metadata.cc
    src/common/to_string.cc
    src/common/to_string.h
    src/common/type_mapping.h
    src/common/transaction.cc
    src/common/transaction_options.cc
    src/common/util.cc
    src/common/util.h
    src/common/write_batch.cc
    ${settings_apple_SRCS})

# Define the resource build needed for Android
firebase_cpp_gradle(":firestore:firestore_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/firestore_resources/build/firestore_resources_lib.jar")
binary_to_array("firestore_resources"
    "${CMAKE_CURRENT_LIST_DIR}/firestore_resources/build/firestore_resources_lib.jar"
    "firebase_firestore"
    "${FIREBASE_GEN_FILE_DIR}/firestore")

set(android_SRCS
    ${firestore_resources_source}
    src/android/aggregate_query_android.cc
    src/android/aggregate_query_android.h
    src/android/aggregate_query_snapshot_android.cc
    src/android/aggregate_query_snapshot_android.h
    src/android/aggregate_source_android.cc
    src/android/aggregate_source_android.h
    src/android/blob_android.cc
    src/android/blob_android.h
    src/android/collection_reference_android.cc
    src/android/collection_reference_android.h
    src/android/direction_android.cc
    src/android/direction_android.h
    src/android/document_change_android.cc
    src/android/document_change_android.h
    src/android/document_change_type_android.cc
    src/android/document_change_type_android.h
    src/android/document_reference_android.cc
    src/android/document_reference_android.h
    src/android/document_snapshot_android.cc
    src/android/document_snapshot_android.h
    src/android/event_listener_android.cc
    src/android/event_listener_android.h
    src/android/exception_android.cc
    src/android/exception_android.h
    src/android/field_path_android.cc
    src/android/field_path_android.h
    src/android/field_path_portable.cc
    src/android/field_path_portable.h
    src/android/field_value_android.cc
    src/android/field_value_android.h
    src/android/firestore_android.cc
    src/android/firestore_android.h
    src/android/firestore_exceptions_android.h
    src/android/geo_point_android.cc
    src/android/geo_point_android.h
    src/android/geo_point_portable.cc
    src/android/jni_runnable_android.cc
    src/android/jni_runnable_android.h
    src/android/lambda_event_listener.h
    src/android/lambda_transaction_function.h
    src/android/listener_registration_android.cc
    src/android/listener_registration_android.h
    src/android/load_bundle_task_android.cc
    src/android/load_bundle_task_android.h
    src/android/load_bundle_task_progress_android.cc
    src/android/load_bundle_task_progress_android.h
    src/android/metadata_changes_android.cc
    src/android/metadata_changes_android.h
    src/android/promise_android.h
    src/android/promise_factory_android.h
    src/android/query_android.cc
    src/android/query_android.h
    src/android/query_snapshot_android.cc
    src/android/query_snapshot_android.h
    src/android/server_timestamp_behavior_android.cc
    src/android/server_timestamp_behavior_android.h
    src/android/set_options_android.cc
    src/android/set_options_android.h
    src/android/settings_android.cc
    src/android/settings_android.h
    src/android/snapshot_metadata_android.cc
    src/android/snapshot_metadata_android.h
    src/android/source_android.cc
    src/android/source_android.h
    src/android/timestamp_android.cc
    src/android/timestamp_android.h
    src/android/timestamp_portable.cc
    src/android/transaction_android.cc
    src/android/transaction_android.h
    src/android/transaction_options_android.cc
    src/android/transaction_options_android.h
    src/android/transaction_options_builder_android.cc
    src/android/transaction_options_builder_android.h
    src/android/util_android.cc
    src/android/util_android.h
    src/android/wrapper.cc
    src/android/wrapper.h
    src/android/write_batch_android.cc
    src/android/write_batch_android.h
    src/jni/array.h
    src/jni/array_list.cc
    src/jni/array_list.h
    src/jni/boolean.cc
    src/jni/boolean.h
    src/jni/call_traits.h
    src/jni/class.cc
    src/jni/class.h
    src/jni/collection.cc
    src/jni/collection.h
    src/jni/compare.h
    src/jni/declaration.h
    src/jni/double.cc
    src/jni/double.h
    src/jni/env.cc
    src/jni/env.h
    src/jni/hash_map.cc
    src/jni/hash_map.h
    src/jni/integer.cc
    src/jni/integer.h
    src/jni/iterator.cc
    src/jni/iterator.h
    src/jni/jni.cc
    src/jni/jni.h
    src/jni/jni_fwd.h
    src/jni/list.cc
    src/jni/list.h
    src/jni/loader.cc
    src/jni/loader.h
    src/jni/long.cc
    src/jni/long.h
    src/jni/map.cc
    src/jni/map.h
    src/jni/object.cc
    src/jni/object.h
    src/jni/ownership.h
    src/jni/set.h
    src/jni/string.cc
    src/jni/string.h
    src/jni/task.cc
    src/jni/task.h
    src/jni/throwable.cc
    src/jni/throwable.h
    src/jni/traits.h)

# Sources that apply to all non-Android platforms.
set(main_SRCS
    src/main/aggregate_query_main.cc
    src/main/aggregate_query_main.h
    src/main/aggregate_query_snapshot_main.cc
    src/main/aggregate_query_snapshot_main.h
    src/main/collection_reference_main.cc
    src/main/collection_reference_main.h
    src/main/converter_main.h
    src/main/document_change_main.cc
    src/main/document_change_main.h
    src/main/document_reference_main.cc
    src/main/document_reference_main.h
    src/main/document_snapshot_main.cc
    src/main/document_snapshot_main.h
    src/main/field_value_main.cc
    src/main/field_value_main.h
    src/main/firestore_main.cc
    src/main/firestore_main.h
    src/main/listener_main.h
    src/main/listener_registration_main.cc
    src/main/listener_registration_main.h
    src/main/promise_factory_main.h
    src/main/promise_main.h
    src/main/query_main.cc
    src/main/query_main.h
    src/main/query_snapshot_main.cc
    src/main/query_snapshot_main.h
    src/main/set_options_main.h
    src/main/source_main.h
    src/main/transaction_main.cc
    src/main/transaction_main.h
    src/main/user_data_converter_main.cc
    src/main/user_data_converter_main.h
    src/main/util_main.h
    src/main/write_batch_main.cc
    src/main/write_batch_main.h)

# CredentialsProvider has no implementatation on Android and distinct
# implementations for iOS and desktop platforms. On iOS, the implementation of
# the interface actually comes from the FirebaseFirestore CocoaPod, so there's
# no implementation listed here.
set(credentials_provider_desktop_SRCS
    src/main/app_check_credentials_provider_desktop.cc
    src/main/app_check_credentials_provider_desktop.h
    src/main/create_app_check_credentials_provider.h
    src/main/create_app_check_credentials_provider_desktop.cc
    src/main/create_credentials_provider.h
    src/main/create_credentials_provider_desktop.cc
    src/main/credentials_provider_desktop.cc
    src/main/credentials_provider_desktop.h)

set(credentials_provider_ios_SRCS
    src/main/create_app_check_credentials_provider.h
    src/main/create_app_check_credentials_provider_ios.mm
    src/main/create_credentials_provider.h
    src/main/create_credentials_provider_ios.mm)

# FirebaseMetadataProvider has no implementation on Android and distinct
# implementations for iOS and desktop platforms. On iOS, the implementation of
# the interface actually comes from the FirebaseFirestore CocoaPod, so there's
# no implementation listed here.
set(firebase_metadata_provider_desktop_SRCS
    src/main/create_firebase_metadata_provider.h
    src/main/create_firebase_metadata_provider_desktop.cc
    src/main/firebase_metadata_provider_desktop.cc
    src/main/firebase_metadata_provider_desktop.h)

set(firebase_metadata_provider_ios_SRCS
    src/main/create_firebase_metadata_provider.h
    src/main/create_firebase_metadata_provider_ios.mm)

set(wrapper_assertions_SRCS
    src/common/wrapper_assertions.cc
    src/common/wrapper_assertions.h)

set(ios_only_SRCS
    src/main/create_firebase_metadata_provider_ios.mm)

set(desktop_only_SRCS
    src/main/create_firebase_metadata_provider_desktop.cc
    src/main/firebase_metadata_provider_desktop.cc)

if(ANDROID)
  set(firestore_platform_SRCS "${android_SRCS}")

elseif(IOS)
  set(firestore_platform_SRCS
      "${main_SRCS}"
      "${credentials_provider_ios_SRCS}"
      "${firebase_metadata_provider_ios_SRCS}")

else()
  set(firestore_platform_SRCS
      "${main_SRCS}"
      "${credentials_provider_desktop_SRCS}"
      "${firebase_metadata_provider_desktop_SRCS}")
endif()

add_library(firebase_firestore STATIC
    ${common_SRCS}
    ${firestore_platform_SRCS})

set_property(TARGET firebase_firestore PROPERTY FOLDER "Firebase Cpp")

# Set up the dependency on Firebase App.
target_link_libraries(firebase_firestore
  PUBLIC
    firebase_app
    firebase_auth
)

if(FIRESTORE_USE_EXTERNAL_CMAKE_BUILD)
  target_link_libraries(firebase_firestore
    PUBLIC
      firestore_core
      absl_variant
  )
endif()

# Public C++ headers all refer to each other relative to the src/include
# directory, while private headers are relative to the entire C++ SDK
# directory.
target_include_directories(firebase_firestore
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
)

# Additional public headers come from the Firestore core
if(IOS)
  target_include_directories(firebase_firestore
    PUBLIC
      # The Firestore core on iOS comes via the FirebaseFirestore CocoaPod, and
      # public headers should come from there.
      ${FIREBASE_POD_DIR}/Pods/FirebaseFirestore/Firestore/core/include
      ${FIREBASE_POD_DIR}/Pods/FirebaseFirestore/Firestore/Protos/nanopb
    PRIVATE
      # Additionally, the core C++ API is not declared publicly within the
      # FirebaseFirestore pod, so depend on headers available in the source
      # distribution.
      ${FIREBASE_POD_DIR}/Pods/FirebaseFirestore
  )
  set(FIREBASE_FIRESTORE_CORE_HEADER_DIR
    ${FIREBASE_POD_DIR}/Pods/FirebaseFirestore/Firestore/core/include
  )
  
else()
  # Desktop and Android get their public headers from the CMake build in
  # firebase-ios-sdk.
  target_include_directories(firebase_firestore
    PUBLIC
      ${FIRESTORE_SOURCE_DIR}/Firestore/core/include
      ${FIRESTORE_SOURCE_DIR}/Firestore/Protos/nanopb
    PRIVATE
      ${FIRESTORE_SOURCE_DIR}
  )
  set(FIREBASE_FIRESTORE_CORE_HEADER_DIR
    ${FIRESTORE_SOURCE_DIR}/Firestore/core/include
  )
  if(DESKTOP)
    target_include_directories(firebase_firestore
      PRIVATE
        ${FLATBUFFERS_SOURCE_DIR}/include
    )
  endif()
endif()

set(firestore_generated_headers_dir
    "${FIREBASE_GEN_FILE_DIR}/firestore/src/include/firebase/firestore")
add_custom_target(
  FIREBASE_FIRESTORE_GENERATED_HEADERS
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${firestore_generated_headers_dir}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${FIREBASE_FIRESTORE_CORE_HEADER_DIR}
          "${firestore_generated_headers_dir}"
  COMMENT "Copying internal Firestore headers"
)

# This is needed due to Firestore's dependence on some firebase::app APIs that
# are guarded by this flag, such as GetUserAgent and function_registry.
set(FIREBASE_FIRESTORE_CPP_DEFINES -DINTERNAL_EXPERIMENTAL=1)

if (MSVC AND NOT ANDROID AND NOT IOS)
  # On Windows, gRPC gives a compiler error in firebase_metadata_provider_desktop.cc
  # unless _WIN32_WINNT is defined to this value (0x0600, Windows Vista).
  # Also set -DNOMINMAX for both Firestore and Firestore Core.
  set(FIREBASE_FIRESTORE_CPP_DEFINES ${FIREBASE_FIRESTORE_CPP_DEFINES} -D_WIN32_WINNT=0x0600 -DNOMINMAX)
  # Special handling for the absl time zone library, define _LIBCPP_VERSION on
  # Windows to avoid the shenanigans they do with Windows mangled symbols to
  # accomplish weak linking. See
  # https://github.com/google/cctz/blob/master/src/zone_info_source.cc for a
  # look at what we're up against.
  target_compile_definitions(absl_time_zone
    PRIVATE
    _LIBCPP_VERSION=99)
endif()

target_compile_definitions(firebase_firestore
  PRIVATE
    ${FIREBASE_FIRESTORE_CPP_DEFINES}
)

# Set the Nanopb preprocessor definitions to match those used by the iOS SDK.
target_compile_definitions(firebase_firestore
  PUBLIC -DPB_FIELD_32BIT -DPB_ENABLE_MALLOC -DPB_NO_PACKED_STRUCTS
)

if(ANDROID)
  firebase_cpp_proguard_file(firestore)

elseif(IOS)
  # Enable Automatic Reference Counting (ARC) and Bitcode.
  target_compile_options(firebase_firestore
                         PUBLIC "-fobjc-arc" "-fembed-bitcode")
  target_link_libraries(firebase_firestore
                        PUBLIC "-fembed-bitcode")

  setup_pod_headers(
    firebase_firestore
    INCLUDE_PRIVATE_HEADERS
    POD_NAMES
      FirebaseCore
      FirebaseFirestore
      abseil
      nanopb
      gRPC-C++
      gRPC-Core
      FirebaseAuthInterop/FirebaseAuth/Interop
      FirebaseCoreExtension/FirebaseCore/Extension
)

  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_firestore PROPERTIES
      FRAMEWORK TRUE
    )
  endif()
endif()

cpp_pack_library(firebase_firestore "")
cpp_pack_public_headers()
