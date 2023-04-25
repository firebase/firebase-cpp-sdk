# Copyright 2018 Google LLC
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

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED YES)  # Don't fall back to an earlier version.

# Turn on virtual folders for visual studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Top level option that determines the default behavior of the include options
# below. Useful for turning off all at once, and then turning on a specific one.
option(FIREBASE_INCLUDE_LIBRARY_DEFAULT
       "Should each library be included by default." ON)
# Different options to enable/disable each library being included during
# configuration.
option(FIREBASE_INCLUDE_ANALYTICS
       "Include the Google Analytics for Firebase library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_APP_CHECK
       "Include the Firebase App Check library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_AUTH "Include the Firebase Authentication library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_DATABASE
       "Include the Firebase Realtime Database library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_DYNAMIC_LINKS
       "Include the Firebase Dynamic Links library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_FIRESTORE
       "Include the Cloud Firestore library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_FUNCTIONS
       "Include the Cloud Functions for Firebase library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_GMA "Include the GMA library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_INSTALLATIONS
       "Include the Firebase Installations library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_MESSAGING
       "Include the Firebase Cloud Messaging library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_REMOTE_CONFIG
       "Include the Firebase Remote Config library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})
option(FIREBASE_INCLUDE_STORAGE
       "Include the Cloud Storage for Firebase library."
       ${FIREBASE_INCLUDE_LIBRARY_DEFAULT})

option(FIREBASE_CPP_BUILD_TESTS
       "Enable the Firebase C++ Build Tests." OFF)
option(FIREBASE_CPP_BUILD_STUB_TESTS
       "Enable the Firebase C++ Build Stub Tests." OFF)
option(FIREBASE_FORCE_FAKE_SECURE_STORAGE
       "Disable use of platform secret store and use fake impl." OFF)
option(FIREBASE_CPP_BUILD_PACKAGE
       "Bundle the Firebase C++ libraries into a zip file." OFF)
option(FIREBASE_CPP_USE_PRIOR_GRADLE_BUILD
       "When building with Gradle, use the previously built libraries." OFF)
option(FIREBASE_USE_BORINGSSL
       "Build against BoringSSL instead of using your system's OpenSSL." OFF)
option(FIREBASE_USE_LINUX_CXX11_ABI
       "Build Linux SDK using the C++11 ABI instead of the legacy ABI." OFF)

# This should only be enabled by the GitHub Action build script.
option(FIREBASE_GITHUB_ACTION_BUILD
       "Indicates that this build was created from a GitHub Action" OFF)

option(FIREBASE_QUICK_TEST
       "Enable quick tests will skip tests which requires access to the SECRET" OFF)

find_program(FIREBASE_PYTHON_EXECUTABLE
  NAMES python3 python
  DOC "The Python interpreter to use, such as one from a venv"
  REQUIRED
)

set(FIREBASE_XCODE_TARGET_FORMAT "frameworks" CACHE STRING
                                 "Format to output, 'frameworks' or 'libraries'")

# Define this directory to be the root of the C++ SDK, which the libraries can
# then refer to.
set(FIREBASE_CPP_SDK_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

project (firebase NONE)
enable_language(C)
enable_language(CXX)

if(NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
  find_program(CCACHE_PROGRAM ccache)
  if (CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "Compiler launcher for C")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "Compiler launcher for CXX")
  endif()
endif()

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(external_rules)
include(cpp_pack)

if(FIREBASE_CPP_BUILD_TESTS OR FIREBASE_CPP_BUILD_STUB_TESTS)
  enable_testing()
  include(test_rules)
  # Copy the custom CTest file into the binary directory, so that it is used.
  configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/CTestCustom.cmake
               ${CMAKE_BINARY_DIR})
endif()

if (PLATFORM STREQUAL TVOS OR PLATFORM STREQUAL SIMULATOR_TVOS)
  #  GMA and FDL are not supported on tvOS.
  set(FIREBASE_INCLUDE_DYNAMIC_LINKS OFF)
  set(FIREBASE_INCLUDE_GMA OFF)
endif()

# Occasionally ANDROID is not being set correctly when invoked by gradle, so
# set it manually if ANDROID_NDK has been defined.
if(DEFINED ANDROID_NDK)
  set(ANDROID ON)
endif()

if(NOT ANDROID AND NOT IOS)
  set(DESKTOP ON)
else()
  set(DESKTOP OFF)
endif()

# For OSX, if the architectures we are building for do not match the current
# host, treat it as cross-compiling.
if(DESKTOP AND APPLE)
  if(NOT "${CMAKE_OSX_ARCHITECTURES}" MATCHES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    message("Cross-compiling ENABLED")
    set(CMAKE_CROSSCOMPILING ON CACHE BOOL "" FORCE)
  endif()
endif()

if(APPLE)
  # For iOS and macOS, disable nullability completeness warning, as it makes the
  # build output for our Objective-C++ files much too verbose.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-nullability-completeness")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nullability-completeness")
endif()

if(DESKTOP AND NOT MSVC AND NOT APPLE)
  # Linux-specific option.
  if (FIREBASE_LINUX_USE_CXX11_ABI)
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=1")
  else()
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")
  endif()
endif()

if(FIREBASE_GITHUB_ACTION_BUILD)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFIREBASE_GITHUB_ACTION_BUILD=1")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFIREBASE_GITHUB_ACTION_BUILD=0")
endif()

# Set directories needed by the Firebase subprojects
# Directory to store generated files.
set(FIREBASE_GEN_FILE_DIR ${CMAKE_BINARY_DIR}/generated)
# Directory for any shared scripts.
set(FIREBASE_SCRIPT_DIR ${CMAKE_CURRENT_LIST_DIR})

if (MSVC)
  if (MSVC_RUNTIME_LIBRARY_STATIC)
    add_compile_options(
      $<$<CONFIG:>:/MT>
      $<$<CONFIG:Debug>:/MTd>
      $<$<CONFIG:Release>:/MT>
  )
  else()
    add_compile_options(
      $<$<CONFIG:>:/MD>
      $<$<CONFIG:Debug>:/MDd>
      $<$<CONFIG:Release>:/MD>
  )
  endif()
endif()

# Set variables that are used by download_external_sources.
set(FIREBASE_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(FIREBASE_BINARY_DIR ${PROJECT_BINARY_DIR})
set(FIREBASE_INSTALL_DIR ${PROJECT_BINARY_DIR}/opt)
set(FIREBASE_DOWNLOAD_DIR ${PROJECT_BINARY_DIR}/downloads)

# Firestore is split into a public C++ component and the Firestore core, a
# non-public C++ API used for building other language bindings. Desktop
# platforms get the Firestore core from the CMake build in firebase-ios-sdk.
#
# Mobile platforms do not:
#
#   * Android uses the Android SDK via JNI and does not use the Firestore
#     core.
#   * iOS gets the Firestore core from the FirebaseFirestore CocoaPod and
#     does not build the core via CMake.
if(FIREBASE_INCLUDE_FIRESTORE AND DESKTOP)
  set(FIRESTORE_USE_EXTERNAL_CMAKE_BUILD ON)
endif()

# Disable compiling the Objective-C (and Swift) stuff from the
# firebase-ios-sdk since it's not needed and can sometimes fail to build.
set(FIRESTORE_INCLUDE_OBJC OFF CACHE BOOL "Disabled for the CPP SDK")

# Disable re2 build tests
set(RE2_BUILD_TESTING OFF CACHE BOOL "")

if(FIREBASE_CPP_USE_PRIOR_GRADLE_BUILD)
  # Quote meta characters in ${CMAKE_CURRENT_LIST_DIR} so we can
  # match it in a regex.
  # For example, '/path/with/+meta/char.acters' will become
  # '/path/with/\+meta/char\.acters'.
  string(REGEX REPLACE
         "([][+.*()^])" "\\\\\\1"  # Yes, this many \'s is correct.
         current_list_dir_regex
         "${CMAKE_CURRENT_LIST_DIR}")
  # Figure out where app's binary_dir was.
  string(REGEX REPLACE
    "${current_list_dir_regex}/[^/]+/(.*)"
    "${CMAKE_CURRENT_LIST_DIR}/app/\\1"
    APP_BINARY_DIR "${FIREBASE_BINARY_DIR}")

  set(FIRESTORE_SOURCE_DIR ${APP_BINARY_DIR}/external/src/firestore)
else()
  # Run the CMake build logic that will download all the external dependencies.
  message(STATUS "Downloading external project dependencies...")
  download_external_sources()
  message(STATUS "Download complete.")

  set(FIRESTORE_SOURCE_DIR ${FIREBASE_BINARY_DIR}/external/src/firestore)
endif()

if(DESKTOP)
  # Use the static versions of the OpenSSL libraries.
  set(OPENSSL_USE_STATIC_LIBS TRUE)
  if (MSVC)
    # Get the correct version of the OpenSSL libraries based on building for MT.
    if ("${CMAKE_CXX_FLAGS_DEBUG}" MATCHES "/MTd" OR
        "${CMAKE_CXX_FLAGS_RELEASE}" MATCHES "/MT")
      set(OPENSSL_MSVC_STATIC_RT TRUE)
    else()
      set(OPENSSL_MSVC_STATIC_RT FALSE)
    endif()
  endif()

  if(FIREBASE_USE_BORINGSSL)
    # Use BoringSSL instead of OpenSSL.
    set(BORINGSSL_ROOT_DIR ${PROJECT_BINARY_DIR}/external/src/boringssl/src CACHE STRING "" FORCE)
    set(BORINGSSL_BINARY_DIR ${PROJECT_BINARY_DIR}/external/src/boringssl-build CACHE STRING "" FORCE)
    set(OPENSSL_ROOT_DIR ${BORINGSSL_ROOT_DIR} CACHE STRING "" FORCE)

    # The call below to build_external_dependencies will make sure that these
    # libraries exist before the libraries are imported via add_library.
    if (MSVC)
      if (CMAKE_BUILD_TYPE)
        set(BORINGSSL_LIB_SUBDIR "${CMAKE_BUILD_TYPE}")
      else()
        set(BORINGSSL_LIB_SUBDIR "Debug")
      endif()
      set(OPENSSL_SSL_LIBRARY ${BORINGSSL_BINARY_DIR}/ssl/${BORINGSSL_LIB_SUBDIR}/ssl.lib CACHE FILEPATH "" FORCE)
      set(OPENSSL_CRYPTO_LIBRARY ${BORINGSSL_BINARY_DIR}/crypto/${BORINGSSL_LIB_SUBDIR}/crypto.lib CACHE FILEPATH "" FORCE)
    else()
      set(OPENSSL_SSL_LIBRARY ${BORINGSSL_BINARY_DIR}/ssl/libssl.a CACHE FILEPATH "" FORCE)
      set(OPENSSL_CRYPTO_LIBRARY ${BORINGSSL_BINARY_DIR}/crypto/libcrypto.a CACHE FILEPATH "" FORCE)
    endif()
  endif()
endif()


if(DESKTOP)
  message(STATUS "Building external project dependencies...")
  build_external_dependencies()
  message(STATUS "Build of external project dependencies complete.")
  
  if(FIREBASE_USE_BORINGSSL)
    set(OPENSSL_FOUND TRUE CACHE BOOL "" FORCE)
    set(OPENSSL_NO_ASM TRUE)  # Force cross-platform BoringSSL, no ASM.
    set(OPENSSL_INCLUDE_DIR ${BORINGSSL_ROOT_DIR}/include CACHE PATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARIES ${OPENSSL_CRYPTO_LIBRARY})
    set(OPENSSL_SSL_LIBRARIES ${OPENSSL_SSL_LIBRARY})
    set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARIES})
    set(OPENSSL_VERSION '1.1.0' CACHE STRING "" FORCE)
  
    add_library(OpenSSL::SSL STATIC IMPORTED)
    add_library(OpenSSL::Crypto STATIC IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
    )
  
    set_target_properties(OpenSSL::Crypto PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
    )
    # Now if we do find_package(OpenSSL) it should give us BoringSSL.
    find_package(OpenSSL)
  
    if(NOT "${OPENSSL_INCLUDE_DIR}" MATCHES boringssl OR
       NOT "${OPENSSL_SSL_LIBRARY}" MATCHES boringssl OR
       NOT "${OPENSSL_CRYPTO_LIBRARY}" MATCHES boringssl)
      message(FATAL_ERROR "BoringSSL was not configured correctly.\nINCLUDE_DIR=${OPENSSL_INCLUDE_DIR}\nSSL_LIBRARY=${OPENSSL_SSL_LIBRARY}\nCRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}")
    endif()
  else()
    # Don't use BoringSSL, use OpenSSL. If you are linking against the libraries directly
    # from source, you probably want this instead.
    #
    # If the find_package fails to find OpenSSL, set OPENSSL_ROOT_DIR to OpenSSL'S install
    # location on your system.
    find_package(OpenSSL REQUIRED)
  endif()
endif()

# Include Firestore's external build early to resolve conflicts on packages.
if(FIRESTORE_USE_EXTERNAL_CMAKE_BUILD)
  if(APPLE)
    # On Apple, set GRPC_BAZEL_BUILD to fix an issue with __thread.
    # Setting this define makes gRPC use pthread's thread-local storage
    # instead of GCC's.
    add_definitions(-DGRPC_BAZEL_BUILD=1)
  endif()
  set(FIRESTORE_BINARY_DIR ${FIRESTORE_SOURCE_DIR}-build)

  if(FIREBASE_CPP_BUILD_TESTS OR FIREBASE_CPP_BUILD_STUB_TESTS)
    set(FIREBASE_IOS_BUILD_TESTS ON
    CACHE BOOL "Force Firestore build tests to match")
  else()
    set(FIREBASE_IOS_BUILD_TESTS OFF
    CACHE BOOL "Force Firestore build tests to match")
  endif()

  add_subdirectory(${FIRESTORE_SOURCE_DIR} ${FIRESTORE_BINARY_DIR})

  copy_subdirectory_definition(${FIRESTORE_SOURCE_DIR} NANOPB_SOURCE_DIR)
endif()

# Disable the Flatbuffer build tests, install and flathash
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")
set(FLATBUFFERS_INSTALL OFF CACHE BOOL "")
set(FLATBUFFERS_FLATHASH OFF CACHE BOOL "")

# Disable LevelDB build tests
set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "")

# Disable building flatc if cross compiling
if(IOS OR ANDROID OR CMAKE_CROSSCOMPILING)
  set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "")
endif()

if(FIREBASE_CPP_USE_PRIOR_GRADLE_BUILD)
  message(STATUS "flatbuffers is added with APP_BINARY_DIR ${APP_BINARY_DIR}")
  add_external_library(flatbuffers BINARY_DIR "${APP_BINARY_DIR}")
else()
  message(STATUS "flatbuffers is added normally")
  add_external_library(flatbuffers)
endif()

if(FIREBASE_CPP_BUILD_TESTS OR FIREBASE_CPP_BUILD_STUB_TESTSAND)
  if (NOT FIRESTORE_USE_EXTERNAL_CMAKE_BUILD)
    # Firestore's external build pulls in GoogleTest
    add_external_library(googletest)
  endif()
endif()

if((FIREBASE_INCLUDE_DATABASE AND DESKTOP) AND NOT FIREBASE_INCLUDE_FIRESTORE)
  # LevelDB is needed for Desktop and Firestore, but if firestore is being built
  # LevelDB will already be included.
  add_external_library(leveldb)
endif()

# Some of the external libraries are not used for mobile.
if(DESKTOP)
  # Build curl as a static library
  set(CURL_STATICLIB ON CACHE BOOL "")
  if (WIN32)
    # Enable Windows native SSL/TLS in libcurl.
    set(CMAKE_USE_SCHANNEL ON CACHE BOOL "")
  endif()

  # Current Curl library defaults to requiring some dependencies we don't need, disable them.
  set(CMAKE_USE_LIBSSH2 OFF)
  set(HTTP_ONLY ON)
  set(BUILD_TESTING OFF)
  if(APPLE AND CMAKE_CROSSCOMPILING AND NOT CMAKE_TOOLCHAIN_FILE)
    # curl's CMake file checks for CMAKE_TOOLCHAIN_FILE rather than checking
    # CMAKE_CROSSCOMPILING when determining whether we are cross-compiling.
    # So if CMAKE_CROSSCOMPILING is set and we don't have a CMAKE_TOOLCHAIN_FILE,
    # just add a null one now so curl is happy. This only happens on Mac when
    # building for arm64/x86_64 on the opposite machine.
    set(CMAKE_TOOLCHAIN_FILE /dev/null)
  endif()
  add_external_library(curl)

  add_external_library(libuv)

  add_external_library(zlib)

  add_external_library(uWebSockets)

  # Binutils on Mac doesn't support thread-local storage (required by
  # websockets), but because we only use websockets via the scheduler, we don't
  # need it. Deactivate this by blanking out the __thread keyword.
  set(websockets_additional_defines "-D__thread=")

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
        ${websockets_additional_defines}
        -DWIN32_LEAN_AND_MEAN # Ensure that windows doesn't include winsock.h by
                              # default, as it can cause issues when libraries try
                              # to include winsock2.h later on in the process.
        -DUSE_LIBUV=1
      )
    elseif(APPLE)
      set(websockets_additional_defines
        ${websockets_additional_defines}
        -DUSE_LIBUV=1
      )
    endif()

    target_compile_definitions(libuWS
      PRIVATE
        ${websockets_additional_defines}
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
        OpenSSL::SSL
        OpenSSL::Crypto
        uv_a
        zlibstatic
    )
  endif()
endif()

if(DESKTOP AND NOT CMAKE_CROSSCOMPILING)
  # Desktop platforms do not need to declare this dependency, as they will build
  # flatc correctly when needed.
  set(FIREBASE_FLATBUFFERS_DEPENDENCIES "")
else()
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

  # Setup the flatc custom build target.
  # These commands are executed from within the current context, which has set
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

  # The cmake executable to use when compiling flatc. Normally, the default
  # value (finding cmake in the PATH) is fine; however, in cases where cmake is
  # *not* in the PATH (e.g. when building from CLion using the Android
  # toolchain) it must be specified via via this cache variable.
  set(FIREBASE_FLATC_CMAKE_COMMAND "cmake" CACHE FILEPATH
    "The cmake command to use when compiling flatc.")

  # Build flatc by invoking the cmake build, with only the flatc target.
  file(MAKE_DIRECTORY ${firebase_external_flatc_build_dir})
  add_custom_command(
    OUTPUT ${firebase_external_flatc}
    COMMAND cd ${firebase_external_flatc_build_dir} ${COMMAND_CONCAT}
            ${ENV_COMMAND} ${FIREBASE_FLATC_CMAKE_COMMAND} ${FLATBUFFERS_SOURCE_DIR} ${COMMAND_CONCAT}
            ${ENV_COMMAND} ${FIREBASE_FLATC_CMAKE_COMMAND} --build . --target flatc
    COMMENT "Building flatc (the FlatBuffer schema compiler)")

  # Add a target so that we can run the custom commands before the code build.
  add_custom_target(
    firebase_flatc_prebuild
    DEPENDS ${firebase_external_flatc})

  # Set a variable that the dependencies can use, to trigger the build before
  # using flatbuffers.
  set(FIREBASE_FLATBUFFERS_DEPENDENCIES "firebase_flatc_prebuild")
endif()

include(binary_to_array)
include(firebase_cpp_gradle)

# Ensure min/max macros don't get declared on Windows
# (so we can use std::min/max), before including the Firebase subdirectories.
if(MSVC)
  add_definitions(-DNOMINMAX)
endif()

add_subdirectory(ios_pod)

# If we're building tests, we need to include the 'testing' folder before any
# of the tests actually get built.
if(FIREBASE_CPP_BUILD_TESTS OR FIREBASE_CPP_BUILD_STUB_TESTS)
  add_subdirectory(testing)
endif()

# Custom target containing all generated headers, used to generate docs only.
add_custom_target(FIREBASE_GENERATED_HEADERS)

if(NOT FIREBASE_CPP_USE_PRIOR_GRADLE_BUILD)
  add_subdirectory(app)
  add_dependencies(FIREBASE_GENERATED_HEADERS FIREBASE_APP_GENERATED_HEADERS)
else()
  # Add firebase_app as a target on the previously built app.
  add_library(firebase_app STATIC IMPORTED GLOBAL)
  file(MAKE_DIRECTORY "${APP_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY "${FIREBASE_BINARY_DIR}/generated")
  set(app_include_dirs
      "${CMAKE_CURRENT_LIST_DIR}/app/src/include"
      "${APP_BINARY_DIR}/generated"
      "${FIREBASE_BINARY_DIR}/generated"
  )
  set_target_properties(firebase_app PROPERTIES
    IMPORTED_LOCATION "${APP_BINARY_DIR}/libfirebase_app.a"
    INTERFACE_INCLUDE_DIRECTORIES "${app_include_dirs}"
  )
endif()

if (FIREBASE_INCLUDE_ANALYTICS)
  add_subdirectory(analytics)
  add_dependencies(FIREBASE_GENERATED_HEADERS FIREBASE_ANALYTICS_GENERATED_HEADERS)
endif()
if (FIREBASE_INCLUDE_APP_CHECK)
  add_subdirectory(app_check)
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
if (FIREBASE_INCLUDE_FIRESTORE)
  add_subdirectory(firestore)
  add_dependencies(FIREBASE_GENERATED_HEADERS FIREBASE_FIRESTORE_GENERATED_HEADERS)
endif()
if (FIREBASE_INCLUDE_FUNCTIONS)
  add_subdirectory(functions)
endif()
if (FIREBASE_INCLUDE_GMA)
  add_subdirectory(gma)
endif()
if (FIREBASE_INCLUDE_INSTALLATIONS)
  add_subdirectory(installations)
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

# Place the CMake and gradle build files provided to easily link against the
# prebuilt libraries at the root of the package.
cpp_pack_dir(
  "${CMAKE_CURRENT_LIST_DIR}/release_build_files/"
  .
)
