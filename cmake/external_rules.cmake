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

# Executes a CMake build of the external dependencies, which will pull them
# as ExternalProjects, under an "external" subdirectory.
function(download_external_sources)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/external)

  # Setup cmake environment.
  # These commands are executed from within the currect context, which has set
  # variables for the target platform. We use "env -i" to clear these
  # variables, and manually keep the PATH to regular bash path.
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    # Windows doesn't have an 'env' command
    set(ENV_COMMAND "")
  else()
    set(firebase_command_line_path "$ENV{PATH}")
    set(ENV_COMMAND env -i PATH=${firebase_command_line_path})
  endif()

  if(IOS)
    set(external_platform "IOS")
  elseif(ANDROID)
    set(external_platform "ANDROID")
  else()
    set(external_platform "DESKTOP")
  endif()

  # Set variables to indicate if local versions of third party libraries should
  # be used instead of downloading them.
  function(check_use_local_directory NAME)
    if (EXISTS ${${NAME}_SOURCE_DIR})
      set(DOWNLOAD_${NAME} OFF PARENT_SCOPE)
    else()
      set(DOWNLOAD_${NAME} ON PARENT_SCOPE)
    endif()
  endfunction()
  check_use_local_directory(BORINGSSL)
  check_use_local_directory(CURL)
  check_use_local_directory(FLATBUFFERS)
  check_use_local_directory(LIBUV)
  check_use_local_directory(UWEBSOCKETS)
  check_use_local_directory(ZLIB)
  check_use_local_directory(FIREBASE_IOS_SDK)

  if(FIREBASE_CPP_BUILD_TESTS OR FIREBASE_CPP_BUILD_STUB_TESTS)
    set(FIREBASE_DOWNLOAD_GTEST ON)
  else()
    set(FIREBASE_DOWNLOAD_GTEST OFF)
  endif()

  # If a GITHUB_TOKEN is present, use it for all external project downloads.
  # This will prevent GitHub runners from being throttled by GitHub.
  if(DEFINED ENV{GITHUB_TOKEN})
    set(EXTERNAL_PROJECT_HTTP_HEADER "Authorization: token $ENV{GITHUB_TOKEN}")
    message("Adding GITHUB_TOKEN header to ExternalProject downloads.")
  else()
    set(EXTERNAL_PROJECT_HTTP_HEADER "")
  endif()

  execute_process(
    COMMAND
      ${ENV_COMMAND} ${CMAKE_COMMAND}
      -G ${CMAKE_GENERATOR}
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
      -DPLATFORM=${PLATFORM}
      -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
      -DCMAKE_INSTALL_PREFIX=${FIREBASE_INSTALL_DIR}
      -DFIREBASE_DOWNLOAD_DIR=${FIREBASE_DOWNLOAD_DIR}
      -DFIREBASE_EXTERNAL_PLATFORM=${external_platform}
      -DDOWNLOAD_BORINGSSL=${DOWNLOAD_BORINGSSL}
      -DDOWNLOAD_CURL=${DOWNLOAD_CURL}
      -DDOWNLOAD_FLATBUFFERS=${DOWNLOAD_FLATBUFFERS}
      -DDOWNLOAD_GOOGLETEST=${FIREBASE_DOWNLOAD_GTEST}
      -DDOWNLOAD_LIBUV=${DOWNLOAD_LIBUV}
      -DDOWNLOAD_UWEBSOCKETS=${DOWNLOAD_UWEBSOCKETS}
      -DDOWNLOAD_ZLIB=${DOWNLOAD_ZLIB}
      -DDOWNLOAD_FIREBASE_IOS_SDK=${DOWNLOAD_FIREBASE_IOS_SDK}
      -DEXTERNAL_PROJECT_HTTP_HEADER=${EXTERNAL_PROJECT_HTTP_HEADER}
      -DFIREBASE_CPP_BUILD_TESTS=${FIREBASE_CPP_BUILD_TESTS}
      -DFIREBASE_CPP_BUILD_STUB_TESTS=${FIREBASE_CPP_BUILD_STUB_TESTS}
      -DFIRESTORE_DEP_SOURCE=${FIRESTORE_DEP_SOURCE}
      ${PROJECT_SOURCE_DIR}/cmake/external
    OUTPUT_FILE ${PROJECT_BINARY_DIR}/external/output_cmake_config.txt
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/external
  )

  # Run downloads in parallel if we know how
  if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set(cmake_build_args -j)
  endif()

  execute_process(
    COMMAND ${ENV_COMMAND} ${CMAKE_COMMAND} --build . -- ${cmake_build_args}
    OUTPUT_FILE ${PROJECT_BINARY_DIR}/external/output_cmake_build.txt
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/external
  )

  # On desktop, make a few tweaks to the downloaded files.
  if(NOT ANDROID AND NOT IOS)
    if (FIREBASE_USE_BORINGSSL)
      # CMake's find_package(OpenSSL) doesn't quite work right with BoringSSL
      # unless the header file contains OPENSSL_VERSION_NUMBER.
      file(READ ${PROJECT_BINARY_DIR}/external/src/boringssl/src/include/openssl/opensslv.h TMP_HEADER_CONTENTS)
      if (NOT TMP_HEADER_CONTENTS MATCHES OPENSSL_VERSION_NUMBER)
        file(APPEND ${PROJECT_BINARY_DIR}/external/src/boringssl/src/include/openssl/opensslv.h
        "\n#ifndef OPENSSL_VERSION_NUMBER\n# define OPENSSL_VERSION_NUMBER  0x10010107L\n#endif\n")
      endif()
      # Also add an #include <stdlib.h> since openssl has it and boringssl
      # doesn't, and some of our code depends on the transitive dependency (this
      # is a bug).
      file(READ ${PROJECT_BINARY_DIR}/external/src/boringssl/src/include/openssl/rand.h TMP_HEADER2_CONTENTS)
      if (NOT TMP_HEADER2_CONTENTS MATCHES "<stdlib.h>")
        file(APPEND ${PROJECT_BINARY_DIR}/external/src/boringssl/src/include/openssl/rand.h
        "\n#include <stdlib.h>\n")
      endif()
    endif()
  endif()
endfunction()

# Builds a subset of external dependencies that need to be built before everything else.
function(build_external_dependencies)
  # Setup cmake environment.
  # These commands are executed from within the currect context, which has set
  # variables for the target platform. We use "env -i" to clear these
  # variables, and manually keep the PATH to regular bash path.
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    # Windows doesn't have an 'env' command
    set(ENV_COMMAND "")
  else()
    set(firebase_command_line_path "$ENV{PATH}")
    set(firebase_command_line_home "$ENV{HOME}")
    set(ENV_COMMAND env -i PATH=${firebase_command_line_path} HOME=${firebase_command_line_home} )
  endif()

  message(STATUS "CMake generator: ${CMAKE_GENERATOR}")
  message(STATUS "CMake generator platform: ${CMAKE_GENERATOR_PLATFORM}")
  message(STATUS "CMake toolchain file: ${CMAKE_TOOLCHAIN_FILE}")

  set(CMAKE_SUB_CONFIGURE_OPTIONS -G "${CMAKE_GENERATOR}")
  set(CMAKE_SUB_BUILD_OPTIONS)

  if (CMAKE_BUILD_TYPE)
    # If Release or Debug were specified, pass it along.
    set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
  endif()
  
  if (CMAKE_VERBOSE_MAKEFILE)
    # If verbose mode was enabled, pass it on
    set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
        "-DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}")
  endif()

  if(APPLE)
    # Propagate MacOS build flags.
    if(CMAKE_OSX_ARCHITECTURES)
      set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
          -DCMAKE_OSX_ARCHITECTURES="${CMAKE_OSX_ARCHITECTURES}")
    endif()
  elseif(MSVC)
    # Propagate MSVC build flags.
    set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
        -A "${CMAKE_GENERATOR_PLATFORM}")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(CMAKE_SUB_BUILD_OPTIONS ${CMAKE_SUB_BUILD_OPTIONS}
          --config Debug)
    else()
      set(CMAKE_SUB_BUILD_OPTIONS ${CMAKE_SUB_BUILD_OPTIONS}
          --config Release)
    endif()
    if(MSVC_RUNTIME_LIBRARY_STATIC)
      set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
          -DCMAKE_C_FLAGS_RELEASE="/MT"
          -DCMAKE_CXX_FLAGS_RELEASE="/MT"
          -DCMAKE_C_FLAGS_DEBUG="/MTd"
          -DCMAKE_CXX_FLAGS_DEBUG="/MTd")
      if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
            -DCMAKE_C_FLAGS="/MTd"
            -DCMAKE_CXX_FLAGS="/MTd"
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug)
      else()
        set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
            -DCMAKE_C_FLAGS="/MT"
            -DCMAKE_CXX_FLAGS="/MT"
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded)
      endif()
    else()  # dynamic (DLL) runtime
      set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
          -DCMAKE_C_FLAGS_RELEASE="/MD"
          -DCMAKE_CXX_FLAGS_RELEASE="/MD"
          -DCMAKE_C_FLAGS_DEBUG="/MDd"
          -DCMAKE_CXX_FLAGS_DEBUG="/MDd")
      if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
            -DCMAKE_C_FLAGS="/MDd"
            -DCMAKE_CXX_FLAGS="/MDd"
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL)
      else()
        set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
            -DCMAKE_C_FLAGS="/MD"
            -DCMAKE_CXX_FLAGS="/MD"
            -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL)
      endif()
    endif()
  else()
    # Propagate Linux build flags.
    get_directory_property(CURR_DIRECTORY_DEFS COMPILE_DEFINITIONS)
    if("${CURR_DIRECTORY_DEFS}" MATCHES "_GLIBCXX_USE_CXX11_ABI=0" OR
       "${CMAKE_CXX_FLAGS}" MATCHES "-D_GLIBCXX_USE_CXX11_ABI=0")
      set(SUBBUILD_USE_CXX11_ABI 0)
    else()
      set(SUBBUILD_USE_CXX11_ABI 1)
    endif()
    set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
          -DCMAKE_C_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=${SUBBUILD_USE_CXX11_ABI}"
          -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=${SUBBUILD_USE_CXX11_ABI}")
    if(CMAKE_TOOLCHAIN_FILE)
      set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    endif()
  endif()
  # Propagate the PIC setting, as the dependencies need to match it
  set(CMAKE_SUB_CONFIGURE_OPTIONS ${CMAKE_SUB_CONFIGURE_OPTIONS}
      -DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE})
  message(STATUS "Sub-configure options: ${CMAKE_SUB_CONFIGURE_OPTIONS}")
  message(STATUS "Sub-build options: ${CMAKE_SUB_BUILD_OPTIONS}")

  if(NOT ANDROID AND NOT IOS)
    if (FIREBASE_USE_BORINGSSL)
      execute_process(
        COMMAND ${ENV_COMMAND} cmake -DOPENSSL_NO_ASM=TRUE ${CMAKE_SUB_CONFIGURE_OPTIONS} ../boringssl/src
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/external/src/boringssl-build
        RESULT_VARIABLE boringssl_configure_status
      )
      if (boringssl_configure_status AND NOT boringssl_configure_status EQUAL 0)
        message(FATAL_ERROR "BoringSSL configure failed: ${boringssl_configure_status}")
      endif()
    
      # Run builds in parallel if we know how
      if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
        set(cmake_build_args -j)
      endif()
    
      execute_process(
        COMMAND ${ENV_COMMAND} cmake --build . ${CMAKE_SUB_BUILD_OPTIONS} --target ssl crypto -- ${cmake_build_args}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/external/src/boringssl-build
        RESULT_VARIABLE boringssl_build_status
      )
      if (boringssl_build_status AND NOT boringssl_build_status EQUAL 0)
        message(FATAL_ERROR "BoringSSL build failed: ${boringssl_build_status}")
      endif()
  
      # Also copy the built files into OPENSSL_ROOT_DIR to handle misconfigured
      # subprojects.
      file(INSTALL "${OPENSSL_CRYPTO_LIBRARY}" DESTINATION "${OPENSSL_ROOT_DIR}")
      file(INSTALL "${OPENSSL_SSL_LIBRARY}" DESTINATION "${OPENSSL_ROOT_DIR}")
    endif()
  endif()
endfunction()

# Populates directory variables for the given name to the location that name
# would be in after a call to download_external_sources, if the variable is
# not already a valid directory.
# Adds the source directory as a subdirectory if a CMakeLists file is found.
function(add_external_library NAME)
  cmake_parse_arguments(optional "" "BINARY_DIR" "" ${ARGN})

  if(optional_BINARY_DIR)
      set(BINARY_DIR "${optional_BINARY_DIR}")
  else()
      set(BINARY_DIR "${FIREBASE_BINARY_DIR}")
  endif()

  string(TOUPPER ${NAME} UPPER_NAME)
  if (NOT EXISTS ${${UPPER_NAME}_SOURCE_DIR})
    set(${UPPER_NAME}_SOURCE_DIR "${BINARY_DIR}/external/src/${NAME}")
    set(${UPPER_NAME}_SOURCE_DIR "${${UPPER_NAME}_SOURCE_DIR}" PARENT_SCOPE)
  endif()

  if (NOT EXISTS ${${UPPER_NAME}_BINARY_DIR})
    set(${UPPER_NAME}_BINARY_DIR "${${UPPER_NAME}_SOURCE_DIR}-build")
    set(${UPPER_NAME}_BINARY_DIR "${${UPPER_NAME}_BINARY_DIR}" PARENT_SCOPE)
  endif()

  message(STATUS "add_ext... ${UPPER_NAME}_SOURCE_DIR: ${${UPPER_NAME}_SOURCE_DIR}")
  message(STATUS "add_ext... ${UPPER_NAME}_BINARY_DIR: ${${UPPER_NAME}_BINARY_DIR}")

  if (EXISTS "${${UPPER_NAME}_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory(${${UPPER_NAME}_SOURCE_DIR} ${${UPPER_NAME}_BINARY_DIR}
            EXCLUDE_FROM_ALL)
  elseif (EXISTS "${${UPPER_NAME}_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(${${UPPER_NAME}_SOURCE_DIR}/src ${${UPPER_NAME}_BINARY_DIR}
            EXCLUDE_FROM_ALL)
  endif()
endfunction()

# Copies a variable definition from a subdirectory into the parent scope
macro(copy_subdirectory_definition DIRECTORY VARIABLE)
  get_directory_property(
    ${VARIABLE}
    DIRECTORY ${DIRECTORY}
    DEFINITION ${VARIABLE}
  )
endmacro()
