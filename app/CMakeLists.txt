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

# CMake file for the firebase_app library

# Define how to generate google_services_resource_(source/header)
binary_to_array("google_services_resource"
    "${CMAKE_CURRENT_LIST_DIR}/google_services.fbs"
    "firebase::fbs"
    "${FIREBASE_GEN_FILE_DIR}/app")

# Define the resource builds needed for Android
firebase_cpp_gradle(":app:app_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/app_resources/build/app_resources_lib.jar")
binary_to_array("app_resources"
    "${CMAKE_CURRENT_LIST_DIR}/app_resources/build/app_resources_lib.jar"
    "firebase_app"
    "${FIREBASE_GEN_FILE_DIR}/app")
firebase_cpp_gradle(":app:google_api_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/google_api_resources/build/google_api_resources_lib.jar")
binary_to_array("google_api_resources"
    "${CMAKE_CURRENT_LIST_DIR}/google_api_resources/build/google_api_resources_lib.jar"
    "google_api"
    "${FIREBASE_GEN_FILE_DIR}/app")
firebase_cpp_gradle(":app:invites_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/invites_resources/build/invites_resources_lib.jar")
binary_to_array("invites_resources"
    "${CMAKE_CURRENT_LIST_DIR}/invites_resources/build/invites_resources_lib.jar"
    "firebase_invites"
    "${FIREBASE_GEN_FILE_DIR}/app")

# Generate version.h
set(version_header_dir ${FIREBASE_GEN_FILE_DIR}/app/src/include/firebase)
set(version_header ${version_header_dir}/version.h)
add_custom_target(FIREBASE_APP_GENERATED_HEADERS DEPENDS "${version_header}")
file(MAKE_DIRECTORY ${version_header_dir})
add_custom_command(
    OUTPUT ${version_header}
    COMMAND ${FIREBASE_PYTHON_EXECUTABLE} "${FIREBASE_SCRIPT_DIR}/version_header.py"
        "--input_file=${FIREBASE_SCRIPT_DIR}/cpp_sdk_version.json"
        "--output_file=${version_header}"
        "--build_type=released"
    COMMENT "Generating version header"
)

# Build the google_services_generated.h header from the flatbuffer schema file.
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-generated"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
# Because of a bug in the version of Flatbuffers we are pinned to,
# additional flags need to be set.
set(FLATC_ARGS "${FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS}")
build_flatbuffers("${CMAKE_CURRENT_LIST_DIR}/google_services.fbs"
                  ""
                  "app_generated_google_services_includes"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/app"
                  ""
                  "")

set(log_common_SRCS
    src/log.cc
    src/log.h
    src/logger.cc
    src/logger.h)
set(log_common_HDRS)
set(log_android_SRCS
    src/jobject_reference.cc
    src/log_android.cc
    src/log_android_callback.cc
    src/util_android.cc)
set(log_android_HDRS
    src/jobject_reference.h
    src/util_android.h)
set(log_ios_SRCS
    src/log_ios.mm)
set(log_ios_HDRS)
set(log_desktop_SRCS
    src/log_stdio.cc)
set(log_desktop_HDRS)
if(ANDROID)
  set(log_SRCS
      "${log_common_SRCS}"
      "${log_android_SRCS}")
  set(log_HDRS
      "${log_common_HDRS}"
      "${log_android_HDRS}")
elseif(IOS)
  set(log_SRCS
      "${log_common_SRCS}"
      "${log_ios_SRCS}")
  set(log_HDRS
      "${log_common_HDRS}"
      "${log_ios_HDRS}")
else()
  set(log_SRCS
      "${log_common_SRCS}"
      "${log_desktop_SRCS}")
  set(log_HDRS
      "${log_common_HDRS}"
      "${log_desktop_HDRS}")
endif()

set(common_SRCS
    ${google_services_resource_source}
    ${google_services_resource_header}
    src/app_common.cc
    src/app_identifier.cc
    src/app_options.cc
    src/callback.cc
    src/cleanup_notifier.cc
    src/function_registry.cc
    src/future.cc
    src/future_manager.cc
    src/path.cc
    src/reference_counted_future_impl.cc
    src/scheduler.cc
    src/thread_cpp11.cc
    src/thread_pthread.cc
    src/time.cc
    src/secure/user_secure_manager.cc
    src/util.cc
    src/variant.cc
    src/base64.cc)

if (MSVC)
  set(mutex_SRCS src/mutex_windows.cc)
else()
  set(mutex_SRCS src/mutex_pthread.cc)
endif()

set(invites_SRCS
    src/invites/cached_receiver.cc
    src/invites/invites_receiver_internal.cc)

set(app_android_SRCS
    src/app_android.cc
    src/google_play_services/availability_android.cc
    ${app_resources_source}
    ${google_api_resources_source}
    ${invites_resources_source}
    src/invites/android/invites_receiver_internal_android.cc
    src/invites/android/invites_android_helper.cc
    src/uuid.cc)
set(app_ios_SRCS
    src/app_ios.mm
    src/util_apple.mm
    src/util_ios.mm
    src/invites/ios/invites_receiver_internal_ios.mm
    src/invites/ios/invites_ios_startup.mm
    src/uuid_ios_darwin.mm)
if (PLATFORM STREQUAL TVOS OR PLATFORM STREQUAL SIMULATOR_TVOS)
  # TVOS does not have a web browser and does not support dynamic links.
  # Remove these files if we are building for TVOS.
  list(REMOVE_ITEM app_ios_SRCS
    src/invites/ios/invites_receiver_internal_ios.mm
    src/invites/ios/invites_ios_startup.mm)
endif()


# Flatbuffer schemas used by the desktop implementation.
set(desktop_flatbuffers_schema_dir
    "${CMAKE_CURRENT_LIST_DIR}/src/heartbeat/schemas")
set(desktop_flatbuffers_schemas
    "${desktop_flatbuffers_schema_dir}/logged_heartbeats.fbs")
set(FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS
    "--no-union-value-namespacing"
    "--gen-generated"
    "--gen-object-api"
    "--cpp-ptr-type" "flatbuffers::unique_ptr")
# Because of a bug in the version of Flatbuffers we are pinned to,
# additional flags need to be set.
# also see if app_generated_includes will conflict
set(FLATC_ARGS "${FLATBUFFERS_FLATC_SCHEMA_EXTRA_ARGS}")
build_flatbuffers("${desktop_flatbuffers_schemas}"
                  "${CMAKE_CURRENT_LIST_DIR}"
                  "app_generated_includes"
                  "${FIREBASE_FLATBUFFERS_DEPENDENCIES}"
                  "${FIREBASE_GEN_FILE_DIR}/app"
                  ""
                  "")

set(app_desktop_SRCS
    src/app_desktop.cc
    src/invites/stub/invites_receiver_internal_stub.cc
    src/variant_util.cc
    src/heartbeat/date_provider.cc
    src/heartbeat/heartbeat_storage_desktop.cc
    src/heartbeat/heartbeat_controller_desktop.cc
    rest/gzipheader.cc
    rest/zlibwrapper.cc
    ${FIREBASE_GEN_FILE_DIR}/app/logged_heartbeats_generated.h)
if(ANDROID)
  set(app_platform_SRCS
      "${app_android_SRCS}")
elseif(IOS)
  set(app_platform_SRCS
      "${app_ios_SRCS}")
else()
  if(MSVC)
    set(app_desktop_extra_SRCS
        src/secure/user_secure_windows_internal.cc
        src/filesystem_desktop_windows.cc
        src/locale.cc
        src/uuid.cc)
  elseif(APPLE)
    set(app_desktop_extra_SRCS
        src/secure/user_secure_darwin_internal.mm
        src/filesystem_apple.mm
        src/locale_apple.mm
        src/uuid_ios_darwin.mm
        src/util_apple.mm)
  else()
    # Linux requires libsecret.
    pkg_check_modules(LIBSECRET libsecret-1)
    if(NOT LIBSECRET_FOUND)
      message(FATAL_ERROR "Unable to find libsecret, which is needed by \
                           Firebase.  It can be installed on supported \
                           systems via: \
                           apt-get install libsecret-1-dev")
    endif()
    # If building for 32 bit, the include directories might need to be fixed.
    if("${CMAKE_CXX_FLAGS}" MATCHES "-m32")
      string(REGEX REPLACE "x86_64" "i386"
             LIBSECRET_INCLUDE_DIRS "${LIBSECRET_INCLUDE_DIRS}")
    endif()
    set(app_desktop_extra_SRCS
        src/secure/user_secure_linux_internal.cc
        src/filesystem_desktop_linux.cc
        src/locale.cc
        src/uuid.cc)
  endif()

  set(app_platform_SRCS
      "${app_desktop_SRCS}"
      "${app_desktop_extra_SRCS}")
endif()

set(internal_HDRS
    src/include/firebase/app.h
    src/include/firebase/future.h
    src/include/firebase/internal/common.h
    src/include/firebase/internal/future_impl.h
    src/include/firebase/internal/mutex.h
    src/include/firebase/internal/type_traits.h
    src/include/firebase/log.h
    src/include/firebase/util.h
    src/include/firebase/variant.h
    src/include/google_play_services/availability.h
    ${version_header})

set(utility_common_HDRS
    src/app_common.h
    src/assert.h
    src/callback.h
    src/cleanup_notifier.h
    src/embedded_file.h
    src/filesystem.h
    src/function_registry.h
    src/future_manager.h
    src/intrusive_list.h
    src/log.h
    src/optional.h
    src/path.h
    src/pthread_condvar.h
    src/reference_count.h
    src/reference_counted_future_impl.h
    src/scheduler.h
    src/semaphore.h
    src/thread.h
    src/time.h
    src/util.h)
set(utility_android_HDRS)
set(utility_ios_HDRS)
set(utility_desktop_HDRS
    src/variant_util.h
    src/invites/cached_receiver.h
    src/invites/invites_receiver_internal.h
    src/invites/receiver_interface.h
    src/invites/sender_receiver_interface.h)
if(ANDROID)
  set(utility_HDRS
      "${utility_common_HDRS}"
      "${utility_android_HDRS}")
elseif(IOS)
  set(utility_HDRS
      "${utility_common_HDRS}"
      "${utility_ios_HDRS}")
else()
  set(utility_HDRS
      "${utility_common_HDRS}"
      "${utility_desktop_HDRS}")
endif()

set(app_android_HDRS
    ${app_resources_header}
    ${google_api_resources_header}
    ${invites_resources_header}
    src/invites/android/invites_android_helper.h
    src/invites/android/invites_receiver_internal_android.h)
set(app_ios_HDRS
    src/app_ios.h
    src/invites/ios/invites_receiver_internal_ios.h)
set(app_desktop_HDRS
    src/invites/stub/invites_receiver_internal_stub.h)
if(ANDROID)
  set(app_platform_HDRS
      "${app_android_HDRS}")
elseif(IOS)
  set(app_platform_HDRS
      "${app_ios_HDRS}")
else()
  set(app_platform_HDRS
      "${app_desktop_HDRS}")
endif()

if(ANDROID OR IOS)
  # If building for Android or iOS, include the Flatbuffers source files directly.
  set(app_flatbuffers_srcs
      ${FLATBUFFERS_SOURCE_DIR}/src/idl_parser.cpp
      ${FLATBUFFERS_SOURCE_DIR}/src/idl_gen_text.cpp
      ${FLATBUFFERS_SOURCE_DIR}/src/reflection.cpp
      ${FLATBUFFERS_SOURCE_DIR}/src/util.cpp)
  set(app_flatbuffers_lib)
else()
  set(app_flatbuffers_srcs)
  set(app_flatbuffers_lib flatbuffers)
endif()

add_library(firebase_app STATIC
    ${log_SRCS}
    ${log_HDRS}
    ${common_SRCS}
    ${invites_SRCS}
    ${mutex_SRCS}
    ${app_platform_SRCS}
    ${internal_HDRS}
    ${utility_HDRS}
    ${app_platform_HDRS}
    ${FIREBASE_GEN_FILE_DIR}/app/google_services_generated.h
    memory/atomic.h
    meta/move.h
    memory/unique_ptr.h
    memory/shared_ptr.h
    ${app_flatbuffers_srcs})

set_property(TARGET firebase_app PROPERTY FOLDER "Firebase Cpp")

# Disable exceptions in std
if (MSVC)
  target_compile_options(firebase_app PRIVATE /EHs-c-)
else()
  target_compile_options(firebase_app PRIVATE -fno-exceptions)
endif()

target_include_directories(firebase_app
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
    ${FIREBASE_GEN_FILE_DIR}
  PRIVATE
    ${FIREBASE_CPP_SDK_ROOT_DIR}
    ${FLATBUFFERS_SOURCE_DIR}/include
    ${LIBSECRET_INCLUDE_DIRS}
)

if (DESKTOP)
  target_include_directories(firebase_app
    PRIVATE
      ${ZLIB_SOURCE_DIR}/..
      ${ZLIB_BINARY_DIR})
endif()

target_compile_definitions(firebase_app
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
)

if(FIREBASE_QUICK_TEST)
  target_compile_definitions(firebase_app 
      PUBLIC 
          QUICK_CHECK
  )
endif()

# firebase_app has a dependency on flatbuffers, which needs to be included.
target_link_libraries(firebase_app
  PRIVATE
    ${app_flatbuffers_lib}
    ${LIBSECRET_LIBRARIES}
)
# Automatically include headers that might not be declared.
if(MSVC)
  add_definitions(/FI"assert.h" /FI"string.h" /FI"stdint.h")
else()
  add_definitions(-include assert.h -include string.h)
endif()

if(ANDROID)
  firebase_cpp_proguard_file(app)
elseif(IOS)
  # Enable Automatic Reference Counting (ARC) and Bitcode.
  target_compile_options(firebase_app
                         PUBLIC "-fobjc-arc" "-fembed-bitcode")
  target_link_libraries(firebase_app
                        PUBLIC "-fembed-bitcode")

  # Add empty include path to get root include folder '.'
  setup_pod_headers(
    firebase_app
    POD_NAMES
      .
      FirebaseCore
      FirebaseDynamicLinks
      FirebaseInstanceID
  )
else()
  target_link_libraries(firebase_app
    PRIVATE zlibstatic)
endif()

if (NOT ANDROID AND NOT IOS)
  # Add the rest subdirectory, so that other libraries can access it
  add_subdirectory(rest)
endif()

if(FIREBASE_CPP_BUILD_TESTS)
  # Add the tests subdirectory
  add_subdirectory(tests)
endif()

cpp_pack_library(firebase_app "")
cpp_pack_public_headers()
if (NOT ANDROID AND NOT IOS)
  cpp_pack_library(flatbuffers "deps/app/external")
endif()

if (IOS)
  # IOS build framework
  if (FIREBASE_XCODE_TARGET_FORMAT STREQUAL "frameworks")
    set_target_properties(firebase_app PROPERTIES
      FRAMEWORK TRUE
    )
  endif()

  # Generate the analytics header file
  # Copy from analytics/CMakeList.txt
  function(generate_analytics_header OBJC_FILE CPP_FILE)
    add_custom_command(
      OUTPUT ${CPP_FILE}
      COMMAND ${FIREBASE_PYTHON_EXECUTABLE} "${FIREBASE_SOURCE_DIR}/analytics/generate_constants.py"
        "--objc_header=${OBJC_FILE}"
        "--cpp_header=${CPP_FILE}"
      DEPENDS ${OBJC_FILE}
      COMMENT "Generating ${CPP_FILE}"
    )
  endfunction()
  set(analytics_generated_headers_dir
    ${FIREBASE_GEN_FILE_DIR}/analytics/src/include/firebase/analytics)
  file(MAKE_DIRECTORY ${FIREBASE_GEN_FILE_DIR}/analytics/src/include/firebase/analytics)
  generate_analytics_header(
    "${FIREBASE_SOURCE_DIR}/analytics/ios_headers/FIREventNames.h"
    "${analytics_generated_headers_dir}/event_names.h"
  )
  generate_analytics_header(
    "${FIREBASE_SOURCE_DIR}/analytics/ios_headers/FIRParameterNames.h"
    "${analytics_generated_headers_dir}/parameter_names.h"
  )
  generate_analytics_header(
    "${FIREBASE_SOURCE_DIR}/analytics/ios_headers/FIRUserPropertyNames.h"
    "${analytics_generated_headers_dir}/user_property_names.h"
  )
  set(analytics_HDRS
    ${FIREBASE_SOURCE_DIR}/analytics/src/include/firebase/analytics.h
    ${analytics_generated_headers_dir}/event_names.h
    ${analytics_generated_headers_dir}/parameter_names.h
    ${analytics_generated_headers_dir}/user_property_names.h)
  set(app_check_HDRS
    ${FIREBASE_SOURCE_DIR}/app_check/src/include/firebase/app_check.h
    ${FIREBASE_SOURCE_DIR}/app_check/src/include/firebase/app_check/app_attest_provider.h
    ${FIREBASE_SOURCE_DIR}/app_check/src/include/firebase/app_check/debug_provider.h
    ${FIREBASE_SOURCE_DIR}/app_check/src/include/firebase/app_check/device_check_provider.h
    ${FIREBASE_SOURCE_DIR}/app_check/src/include/firebase/app_check/play_integrity_provider.h)
  set(auth_HDRS
    ${FIREBASE_SOURCE_DIR}/auth/src/include/firebase/auth.h
    ${FIREBASE_SOURCE_DIR}/auth/src/include/firebase/auth/credential.h
    ${FIREBASE_SOURCE_DIR}/auth/src/include/firebase/auth/types.h
    ${FIREBASE_SOURCE_DIR}/auth/src/include/firebase/auth/user.h)
  set(database_HDRS
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database.h
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database/common.h
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database/data_snapshot.h
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database/database_reference.h
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database/disconnection.h
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database/listener.h
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database/mutable_data.h
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database/query.h
    ${FIREBASE_SOURCE_DIR}/database/src/include/firebase/database/transaction.h)
  set(dynamic_links_HDRS
    ${FIREBASE_SOURCE_DIR}/dynamic_links/src/include/firebase/dynamic_links.h
    ${FIREBASE_SOURCE_DIR}/dynamic_links/src/include/firebase/dynamic_links/components.h)
  set(firestore_HDRS
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/aggregate_query.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/aggregate_query_snapshot.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/aggregate_source.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/collection_reference.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/document_change.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/document_reference.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/document_snapshot.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/field_path.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/field_value.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/listener_registration.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/load_bundle_task_progress.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/map_field_value.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/metadata_changes.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/query_snapshot.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/query.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/set_options.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/settings.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/snapshot_metadata.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/source.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/transaction.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/transaction_options.h
    ${FIREBASE_SOURCE_DIR}/firestore/src/include/firebase/firestore/write_batch.h
    ${FIREBASE_POD_DIR}/Pods/FirebaseFirestore/Firestore/core/include/firebase/firestore/firestore_errors.h
    ${FIREBASE_POD_DIR}/Pods/FirebaseFirestore/Firestore/core/include/firebase/firestore/firestore_version.h
    ${FIREBASE_POD_DIR}/Pods/FirebaseFirestore/Firestore/core/include/firebase/firestore/geo_point.h
    ${FIREBASE_POD_DIR}/Pods/FirebaseFirestore/Firestore/core/include/firebase/firestore/timestamp.h)
  set(functions_HDRS
    ${FIREBASE_SOURCE_DIR}/functions/src/include/firebase/functions.h
    ${FIREBASE_SOURCE_DIR}/functions/src/include/firebase/functions/callable_reference.h
    ${FIREBASE_SOURCE_DIR}/functions/src/include/firebase/functions/callable_result.h
    ${FIREBASE_SOURCE_DIR}/functions/src/include/firebase/functions/common.h)
  set(gma_HDRS
    ${FIREBASE_SOURCE_DIR}/gma/src/include/firebase/gma.h
    ${FIREBASE_SOURCE_DIR}/gma/src/include/firebase/gma/ad_view.h
    ${FIREBASE_SOURCE_DIR}/gma/src/include/firebase/gma/interstitial_ad.h
    ${FIREBASE_SOURCE_DIR}/gma/src/include/firebase/gma/rewarded_ad.h
    ${FIREBASE_SOURCE_DIR}/gma/src/include/firebase/gma/types.h)
  set(installations_HDRS
    ${FIREBASE_SOURCE_DIR}/installations/src/include/firebase/installations.h)
  set(messaging_HDRS
    ${FIREBASE_SOURCE_DIR}/messaging/src/include/firebase/messaging.h)
  set(remote_config_HDRS
    ${FIREBASE_SOURCE_DIR}/remote_config/src/include/firebase/remote_config.h
    ${FIREBASE_SOURCE_DIR}/remote_config/src/include/firebase/remote_config/config_update_listener_registration.h)
  set(storage_HDRS
    ${FIREBASE_SOURCE_DIR}/storage/src/include/firebase/storage.h
    ${FIREBASE_SOURCE_DIR}/storage/src/include/firebase/storage/common.h
    ${FIREBASE_SOURCE_DIR}/storage/src/include/firebase/storage/controller.h
    ${FIREBASE_SOURCE_DIR}/storage/src/include/firebase/storage/listener.h
    ${FIREBASE_SOURCE_DIR}/storage/src/include/firebase/storage/metadata.h
    ${FIREBASE_SOURCE_DIR}/storage/src/include/firebase/storage/storage_reference.h)

  list(APPEND framework_HDRS
    src/include/firebase/internal/platform.h
    ${analytics_HDRS}
    ${app_check_HDRS}
    ${auth_HDRS}
    ${database_HDRS}
    ${dynamic_links_HDRS}
    ${firestore_HDRS}
    ${functions_HDRS}
    ${gma_HDRS}
    ${installations_HDRS}
    ${messaging_HDRS}
    ${remote_config_HDRS}
    ${storage_HDRS})

  # add framework header files to target
  target_sources(firebase_app PRIVATE ${framework_HDRS})

  # Generate Framework Headers from ${framework_HDRS}
  # Copy header from source dir "src/include/firebase/*" to destination dir "Headers/*"
  # Firestore has few headers from a different directory pattern (core/include/firebase/*).
  list(APPEND framework_HDRS ${internal_HDRS})
  foreach(hfile ${framework_HDRS})
    string(FIND ${hfile} "/include/firebase/" pos_firebase)
    string(COMPARE NOTEQUAL ${pos_firebase} -1 find)
    if(${find})
      string(FIND ${hfile} "/" pos_e REVERSE)
      math(EXPR pos_s "${pos_firebase} + 17")
      math(EXPR len "${pos_e} - ${pos_s}")
      string(SUBSTRING ${hfile} ${pos_s} ${len} dir)
      # file(COPY ${hfile} DESTINATION Headers/${dir})
      set_property(SOURCE ${hfile} PROPERTY 
                  MACOSX_PACKAGE_LOCATION Headers/${dir})
    endif()
  endforeach(hfile)
  # Copy header from source dir "src/include/google_play_services/*" 
  # to destination dir "Headers/google_play_services/*"
  set_property(SOURCE "src/include/google_play_services/availability.h" PROPERTY 
                  MACOSX_PACKAGE_LOCATION Headers/google_play_services)
endif()
