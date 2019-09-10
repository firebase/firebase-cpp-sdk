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

# Top level CMake file that defines the entire Firebase C++ SDK build.

cmake_minimum_required (VERSION 3.1)
set (CMAKE_CXX_STANDARD 11)

# Turn on virtual folders for visual studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Different options to enable/disable each library being included during
# configuration.
option(FIREBASE_INCLUDE_ADMOB "Include the AdMob library." ON)
option(FIREBASE_INCLUDE_ANALYTICS
       "Include the Google Analytics for Firebase library." ON)
option(FIREBASE_INCLUDE_AUTH "Include the Firebase Authentication library." ON)
option(FIREBASE_INCLUDE_DATABASE
       "Include the Firebase Realtime Database library." ON)
option(FIREBASE_INCLUDE_DYNAMIC_LINKS
       "Include the Firebase Dynamic Links library." ON)
option(FIREBASE_INCLUDE_FUNCTIONS
       "Include the Cloud Functions for Firebase library." ON)
option(FIREBASE_INCLUDE_INSTANCE_ID
       "Include the Firebase Instance ID library." ON)
option(FIREBASE_INCLUDE_MESSAGING
       "Include the Firebase Cloud Messaging library." ON)
option(FIREBASE_INCLUDE_REMOTE_CONFIG
       "Include the Firebase Remote Config library." ON)
option(FIREBASE_INCLUDE_STORAGE
       "Include the Cloud Storage for Firebase library." ON)
option(FIREBASE_CPP_BUILD_TESTS
       "Enable the Firebase C++ Build Tests." OFF)
option(FIREBASE_FORCE_FAKE_SECURE_STORAGE
       "Disable use of platform secret store and use fake impl." OFF)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(external_rules)

if(FIREBASE_CPP_BUILD_TESTS)
  enable_testing()
  include(test_rules)
  # Copy the custom CTest file into the binary directory, so that it is used.
  configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/CTestCustom.cmake
               ${CMAKE_BINARY_DIR})
endif()

# Occasionally ANDROID is not being set correctly when invoked by gradle, so
# set it manually if ANDROID_NDK has been defined.
if(DEFINED ANDROID_NDK)
  set(ANDROID 1)
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT ANDROID)
  # Force using the toolchain file, as otherwise it loads it later in the
  # process, after several of the checks against it have failed.
  if(IS_ABSOLUTE ${CMAKE_TOOLCHAIN_FILE})
    message("Using Toolchain File: ${CMAKE_TOOLCHAIN_FILE}")
    include("${CMAKE_TOOLCHAIN_FILE}")
  else()
    message("Using Toolchain File: ${CMAKE_BINARY_DIR}/${CMAKE_TOOLCHAIN_FILE}")
    include("${CMAKE_BINARY_DIR}/${CMAKE_TOOLCHAIN_FILE}")
  endif()
endif()

# We're on iOS if the system root is set to "iphone" or some variant.
if("${CMAKE_OSX_SYSROOT}" MATCHES "iphone")
  set(IOS TRUE CACHE BOOL "Target platform is iOS.")
endif()

# Set directories needed by the Firebase subprojects
# Directory to store generated files.
set(FIREBASE_GEN_FILE_DIR ${CMAKE_BINARY_DIR}/generated)
# Directory for any shared scripts.
set(FIREBASE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_DIR})

project (firebase NONE)
enable_language(C)
enable_language(CXX)

if (FIREBASE_CPP_BUILD_TESTS AND MSVC)
  # Googletest requires MSVC to compile with the static version of the runtime
  # library, so define the appropriate runtime flag, before adding libraries.
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
endif()

# Set variables that are used by download_external_sources.
set(FIREBASE_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(FIREBASE_BINARY_DIR ${PROJECT_BINARY_DIR})
set(FIREBASE_INSTALL_DIR ${PROJECT_BINARY_DIR}/opt)
set(FIREBASE_DOWNLOAD_DIR ${PROJECT_BINARY_DIR}/downloads)

# Run the CMake build logic that will download all the external dependencies.
download_external_sources()

# Disable the Flatbuffer build tests
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")

# Add flatbuffers as a subdirectory, and set the directory variables for it,
# so that the sub Firebase projects can depend upon it if necessary.
add_external_library(flatbuffers)

if(FIREBASE_CPP_BUILD_TESTS)
  add_external_library(googletest)
  add_external_library(absl)
endif()

# Some of the external libraries are not used for mobile.
if (NOT ANDROID AND NOT IOS)
  # Build curl as a static library
  set(CURL_STATICLIB ON CACHE BOOL "")
  if (WIN32)
    set(CMAKE_USE_WINSSL ON CACHE BOOL "")
  endif()
  add_external_library(curl)

  add_external_library(libuv)

  find_package(OpenSSL)

  add_external_library(zlib)

  add_external_library(uWebSockets)
  # uWebSockets does not come with a CMakeLists file, so define the target.
  # Note that since it depends on OpenSSL, only do so if that was found.
  if (OPENSSL_FOUND)
    if (MSVC OR APPLE)
      set(uWebSockets_extra_src)
    else()
      # Epoll is only used on Linux, otherwise LibUV is used.
      set(uWebSockets_extra_src
          ${UWEBSOCKETS_SOURCE_DIR}/src/Epoll.cpp)
    endif()
    add_library(libuWS STATIC
        ${uWebSockets_extra_src}
        ${UWEBSOCKETS_SOURCE_DIR}/src/Extensions.cpp
        ${UWEBSOCKETS_SOURCE_DIR}/src/Group.cpp
        ${UWEBSOCKETS_SOURCE_DIR}/src/HTTPSocket.cpp
        ${UWEBSOCKETS_SOURCE_DIR}/src/Hub.cpp
        ${UWEBSOCKETS_SOURCE_DIR}/src/Networking.cpp
        ${UWEBSOCKETS_SOURCE_DIR}/src/Node.cpp
        ${UWEBSOCKETS_SOURCE_DIR}/src/Socket.cpp
        ${UWEBSOCKETS_SOURCE_DIR}/src/WebSocket.cpp)
    if(MSVC)
      set(websockets_additional_defines
        -DWIN32_LEAN_AND_MEAN # Ensure that windows doesn't include winsock.h by
                              # default, as it can cause issues when libraries try
                              # to include winsock2.h later on in the process.
        -DUSE_LIBUV=1
        )
    elseif(APPLE)
      set(websockets_additional_defines
        -DUSE_LIBUV=1
      )
    endif()

    target_compile_definitions(libuWS
      PRIVATE
        "${websockets_additional_defines}"
    )
    target_include_directories(libuWS
      PUBLIC
        ${LIBUV_SOURCE_DIR}/include
        ${UWEBSOCKETS_SOURCE_DIR}
        ${ZLIB_SOURCE_DIR}
        ${ZLIB_BINARY_DIR}
      PRIVATE
        ${OPENSSL_INCLUDE_DIR}
    )
    target_link_libraries(libuWS
      PRIVATE
        ${OPENSSL_SSL_LIBRARY}
        uv_a
        zlibstatic
    )
  endif()

  find_package(Protobuf)
  if (PROTOBUF_FOUND)
    # NanoPB requires Protobuf to be present, so only add it if it was found.
    add_external_library(nanopb)
    # NanoPB has a FindNanopb which defines the function to generate files, so
    # add it to the module path, and use that.
    list(INSERT CMAKE_MODULE_PATH 0 ${NANOPB_SOURCE_DIR}/extra)
    find_package(Nanopb)
  endif()
endif()

if(ANDROID OR IOS)
  # Mobile platforms build flatc externally so that it works on the platform
  # performing the build.
  set(firebase_external_flatc_build_dir "${FLATBUFFERS_BINARY_DIR}-flatc")
  set(firebase_external_flatc "${firebase_external_flatc_build_dir}/flatc")

  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    # Fix path slash direction (windows paths use '\' instead of '/')
    string(REPLACE "/" "\\"
                  firebase_external_flatc_build_dir_win
                  ${firebase_external_flatc_build_dir})
    set(firebase_external_flatc_build_dir
        ${firebase_external_flatc_build_dir_win})
    unset(firebase_external_flatc_build_dir_win)

    # Building on Windows places the executable in a /Debug/ folder
    set(firebase_external_flatc
        "${firebase_external_flatc_build_dir}/Debug/flatc")
  endif()

  set(FLATBUFFERS_FLATC_EXECUTABLE ${firebase_external_flatc} CACHE STRING "")
  set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "")

  # Setup the flatc custom build target.
  # These commands are executed from within the currect context, which has set
  # variables for the target platform. We use "env -i" to clear these
  # variables, and manually keep the PATH to regular bash path.
  # If we didn't do this, we'd end up building flatc for the target platform
  # instead of the building platform, and thus couldn't use flatc on the
  # building platform.
  set(firebase_command_line_path "$ENV{PATH}")

  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    # OS - Specific Command Joiner
    set(COMMAND_CONCAT &)

    #Windows doesn't have an 'env' command
    set(ENV_COMMAND "")
  else()
    set(COMMAND_CONCAT &&)
    set(ENV_COMMAND env -i PATH=${firebase_command_line_path})
  endif()

  # Build flatc by invoking the cmake build, with only the flatc target.
  file(MAKE_DIRECTORY ${firebase_external_flatc_build_dir})
  add_custom_command(
    OUTPUT ${firebase_external_flatc}
    COMMAND cd ${firebase_external_flatc_build_dir} ${COMMAND_CONCAT}
            ${ENV_COMMAND} cmake ${FLATBUFFERS_SOURCE_DIR} ${COMMAND_CONCAT}
            ${ENV_COMMAND} cmake --build . --target flatc
    COMMENT "Building flatc (the FlatBuffer schema compiler)")

  # Add a target so that we can run the custom commands before the code build.
  add_custom_target(
    firebase_flatc_prebuild
    DEPENDS ${firebase_external_flatc})

  # Set a variable that the dependencies can use, to trigger the build before
  # using flatbuffers.
  set(FIREBASE_FLATBUFFERS_DEPENDENCIES "firebase_flatc_prebuild")
else()
  # Other platforms do not need to declare this dependency, as they will build
  # flatc correctly when needed.
  set(FIREBASE_FLATBUFFERS_DEPENDENCIES "")
endif()

# Define this directory to be the root of the C++ SDK, which the libraries can
# then refer to.
set(FIREBASE_CPP_SDK_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

# If we're building tests, we need to include the 'testing' folder before any
# of the tests actually get built.
if(FIREBASE_CPP_BUILD_TESTS)
  add_subdirectory(testing)
endif()

# App needs to come first, since other libraries will depend upon it.
add_subdirectory(app)
if (FIREBASE_INCLUDE_ADMOB)
  add_subdirectory(admob)
endif()
if (FIREBASE_INCLUDE_ANALYTICS)
  add_subdirectory(analytics)
endif()
if (FIREBASE_INCLUDE_AUTH)
  add_subdirectory(auth)
endif()
if (FIREBASE_INCLUDE_DATABASE)
  add_subdirectory(database)
endif()
if (FIREBASE_INCLUDE_DYNAMIC_LINKS)
  add_subdirectory(dynamic_links)
endif()
if (FIREBASE_INCLUDE_FUNCTIONS)
  add_subdirectory(functions)
endif()
if (FIREBASE_INCLUDE_INSTANCE_ID)
  add_subdirectory(instance_id)
endif()
if (FIREBASE_INCLUDE_MESSAGING)
  add_subdirectory(messaging)
endif()
if (FIREBASE_INCLUDE_REMOTE_CONFIG)
  add_subdirectory(remote_config)
endif()
if (FIREBASE_INCLUDE_STORAGE)
  add_subdirectory(storage)
endif()
