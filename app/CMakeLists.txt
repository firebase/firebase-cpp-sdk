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
    "${CMAKE_CURRENT_LIST_DIR}/app_resources/build/dexed.jar")
binary_to_array("app_resources"
    "${CMAKE_CURRENT_LIST_DIR}/app_resources/build/dexed.jar"
    "firebase_app"
    "${FIREBASE_GEN_FILE_DIR}/app")
firebase_cpp_gradle(":app:google_api_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/google_api_resources/build/dexed.jar")
binary_to_array("google_api_resources"
    "${CMAKE_CURRENT_LIST_DIR}/google_api_resources/build/dexed.jar"
    "google_api"
    "${FIREBASE_GEN_FILE_DIR}/app")
firebase_cpp_gradle(":app:invites_resources:generateDexJarRelease"
    "${CMAKE_CURRENT_LIST_DIR}/invites_resources/build/dexed.jar")
binary_to_array("invites_resources"
    "${CMAKE_CURRENT_LIST_DIR}/invites_resources/build/dexed.jar"
    "firebase_invites"
    "${FIREBASE_GEN_FILE_DIR}/app")

# Generate build_type_generated.h
file(MAKE_DIRECTORY ${FIREBASE_GEN_FILE_DIR}/app/src)
set(build_type_header ${FIREBASE_GEN_FILE_DIR}/app/src/build_type_generated.h)
add_custom_command(
    OUTPUT ${build_type_header}
    COMMAND python "${FIREBASE_SCRIPT_DIR}/build_type_header.py"
        "--build_type=head"
        "--output_file=${build_type_header}"
    COMMENT "Generating build_type_generated header"
)

# Generate version.h
set(version_header_dir ${FIREBASE_GEN_FILE_DIR}/app/src/include/firebase)
set(version_header ${version_header_dir}/version.h)
file(MAKE_DIRECTORY ${version_header_dir})
add_custom_command(
    OUTPUT ${version_header}
    COMMAND python "${FIREBASE_SCRIPT_DIR}/version_header.py"
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
                  "app_generated_includes"
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
    src/heartbeat_date_storage_desktop.cc
    src/heartbeat_info_desktop.cc
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
    src/util_ios.mm
    src/invites/ios/invites_receiver_internal_ios.mm
    src/invites/ios/invites_ios_startup.mm
    src/uuid_ios_darwin.mm)
set(app_desktop_SRCS
    src/app_desktop.cc
    src/invites/stub/invites_receiver_internal_stub.cc
    src/variant_util.cc)
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
        src/uuid_ios_darwin.mm)
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
    src/include/firebase/internal/type_traits.h
    src/include/firebase/log.h
    src/include/firebase/util.h
    src/include/firebase/variant.h
    src/include/google_play_services/availability.h
    ${version_header})

set(utility_common_HDRS
    src/app_common.h
    src/assert.h
    ${build_type_header}
    src/callback.h
    src/cleanup_notifier.h
    src/embedded_file.h
    src/filesystem.h
    src/function_registry.h
    src/future_manager.h
    src/intrusive_list.h
    src/log.h
    src/heartbeat_date_storage_desktop.h
    src/heartbeat_info_desktop.h
    src/mutex.h
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

add_library(firebase_app STATIC
    ${log_SRCS}
    ${log_HDRS}
    ${common_SRCS}
    ${invites_SRCS}
    ${app_platform_SRCS}
    ${internal_HDRS}
    ${utility_HDRS}
    ${app_platform_HDRS}
    ${FIREBASE_GEN_FILE_DIR}/app/google_services_generated.h
    memory/atomic.h
    meta/move.h
    memory/unique_ptr.h
    memory/shared_ptr.h)

set_property(TARGET firebase_app PROPERTY FOLDER "Firebase Cpp")

# Disable exceptions in std
if (MSVC)
  target_compile_options(firebase_app PUBLIC /EHs-c-)
else()
  target_compile_options(firebase_app PUBLIC -fno-exceptions)
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
target_compile_definitions(firebase_app
  PRIVATE
    -DINTERNAL_EXPERIMENTAL=1
)
# firebase_app has a dependency on flatbuffers, which needs to be included.
target_link_libraries(firebase_app
  PRIVATE
    flatbuffers
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
  # Enable Automatic Reference Counting (ARC).
  set_property(
    TARGET firebase_app
    APPEND_STRING PROPERTY
    COMPILE_FLAGS "-fobjc-arc")

  # Add empty include path to get root include folder '.'
  setup_pod_headers(
    firebase_app
    POD_NAMES
      .
      FirebaseCore
      FirebaseDynamicLinks
      FirebaseInstanceID
  )
endif()

if (NOT ANDROID AND NOT IOS)
  # Add the rest subdirectory, so that other libraries can access it
  add_subdirectory(rest)
  add_subdirectory(instance_id)
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
