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

# Top level CMake file that defines targets for the Firebase C++ SDK.

cmake_minimum_required(VERSION 3.1)

include(FindPkgConfig)

if (NOT DEFINED FIREBASE_CPP_SDK_DIR)
  set(FIREBASE_CPP_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

# Determine the location of the libraries to use based on the platform.
if(ANDROID)
  string(REPLACE "_" ";" SPLIT_STL ${ANDROID_STL})
  list(GET SPLIT_STL 0 STL_DIR)
  set(FIREBASE_SDK_LIBDIR
      "${FIREBASE_CPP_SDK_DIR}/libs/android/${ANDROID_ABI}/${STL_DIR}")
elseif(APPLE)
  set(FIREBASE_SDK_LIBDIR ${FIREBASE_CPP_SDK_DIR}/libs/darwin/universal)
elseif(MSVC)
  if(${CMAKE_CL_64})
    set(MSVC_CPU x64)
  else()
    set(MSVC_CPU x86)
  endif()
  if(CMAKE_BUILD_TYPE STREQUAL Release)
    set(MSVC_CONFIG Release)
  else()
    set(MSVC_CONFIG Debug)
  endif()
  set(MSVC_VS_VERSION VS2015)
  set(FIREBASE_SDK_LIBDIR
      ${FIREBASE_CPP_SDK_DIR}/libs/windows/${MSVC_VS_VERSION}/${MSVC_RUNTIME_MODE}/${MSVC_CPU}/${MSVC_CONFIG})
else()
  # The Firebase libraries are not built with glibcxx11, so disable the ABI.
  set(DISABLE_CXX11 TRUE)
  set(LINUX_CPU x86_64)
  set(FIREBASE_SDK_LIBDIR ${FIREBASE_CPP_SDK_DIR}/libs/linux/${LINUX_CPU})
endif()

# Defines a Firebase target, linking it with the correct prebuilt library.
function(add_firebase_target TARGET_NAME)
  if(NOT DEFINED ${TARGET_NAME})
    if(MSVC)
      set(LIBRARY_NAME "${TARGET_NAME}.lib")
    else()
      set(LIBRARY_NAME "lib${TARGET_NAME}.a")
    endif()
    add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)
    set_target_properties(${TARGET_NAME} PROPERTIES
      IMPORTED_LOCATION "${FIREBASE_SDK_LIBDIR}/${LIBRARY_NAME}"
      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    )
    if(${DISABLE_CXX11})
      set_target_properties(${TARGET_NAME} PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "_GLIBCXX_USE_CXX11_ABI=0"
      )
    endif()
  endif()
endfunction()

# Define targets for all the Firebase libraries
add_firebase_target(firebase_app)
add_firebase_target(firebase_admob)
add_firebase_target(firebase_analytics)
add_firebase_target(firebase_auth)
add_firebase_target(firebase_database)
add_firebase_target(firebase_dynamic_links)
add_firebase_target(firebase_firestore)
add_firebase_target(firebase_functions)
add_firebase_target(firebase_instance_id)
add_firebase_target(firebase_messaging)
add_firebase_target(firebase_remote_config)
add_firebase_target(firebase_storage)

# Auth on Linux desktop has an additional dependency on libsecret,
# which needs to be added. If it cannot be found, we don't want to
# error, since the user might not be using auth, and the user will
# get a linker error if it is needed.
if(NOT APPLE AND NOT ANDROID AND UNIX)
  pkg_check_modules(LIBSECRET libsecret-1)

  if(NOT LIBSECRET_FOUND)
    message(WARNING "Unable to find libsecret, which is needed by Linux \
                     desktop implementations of Auth, Instance ID, and \
                     Remote Config. \
                     It can be installed on supported systems via: \
                     apt-get install libsecret-1-dev")
  else()
    set_target_properties(firebase_auth PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBSECRET_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${LIBSECRET_LIBRARIES}"
    )
    set_target_properties(firebase_instance_id PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBSECRET_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${LIBSECRET_LIBRARIES}"
    )
    set_target_properties(firebase_remote_config PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBSECRET_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${LIBSECRET_LIBRARIES}"
    )
  endif()
endif()
