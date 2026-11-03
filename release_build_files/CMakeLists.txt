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

cmake_minimum_required(VERSION 3.22)

project(FirebasePrebuilt NONE)

include(FindPkgConfig)

if(NOT DEFINED FIREBASE_CPP_SDK_DIR)
  set(FIREBASE_CPP_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

set(DISABLE_CXX11 FALSE)

# Determine the location of the libraries to use based on the platform.
if(ANDROID)
  set(FIREBASE_SDK_LIBDIR
      "${FIREBASE_CPP_SDK_DIR}/libs/android/${ANDROID_ABI}")
elseif(APPLE)
  if(IOS)
    set(FIREBASE_SDK_LIBDIR "${FIREBASE_CPP_SDK_DIR}/libs/ios/universal")
  else() # Assume macOS
    set(FIREBASE_SDK_LIBDIR "${FIREBASE_CPP_SDK_DIR}/libs/darwin/universal")
  endif()
elseif(MSVC)
  if(CMAKE_CL_64 OR CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MSVC_CPU x64)
  else()
    set(MSVC_CPU x86)
  endif()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MSVC_CONFIG Debug)
  else()
    set(MSVC_CONFIG Release)
  endif()
  if(NOT DEFINED MSVC_VS_VERSION)
    set(MSVC_VS_VERSION VS2019)
  endif()
  if(NOT DEFINED MSVC_RUNTIME_MODE)
    set(MSVC_RUNTIME_MODE MD)
  endif()
  set(FIREBASE_SDK_LIBDIR
      "${FIREBASE_CPP_SDK_DIR}/libs/windows/${MSVC_VS_VERSION}/${MSVC_RUNTIME_MODE}/${MSVC_CPU}/${MSVC_CONFIG}")
else()  # LINUX
  # Check whether we are building with CXX11 ABI.
  get_directory_property(CURR_DIRECTORY_DEFS COMPILE_DEFINITIONS)
  if("${CURR_DIRECTORY_DEFS}" MATCHES "_GLIBCXX_USE_CXX11_ABI=0" OR
     "${CMAKE_CXX_FLAGS}" MATCHES "-D_GLIBCXX_USE_CXX11_ABI=0")
    set(DISABLE_CXX11 TRUE)
    set(LINUX_ABI legacy)
  else()
    set(DISABLE_CXX11 FALSE)
    set(LINUX_ABI cxx11)
  endif()
  # Check target CPU architecture on Linux.
  if(" ${CMAKE_CXX_FLAGS} " MATCHES " -m32 ")
    set(LINUX_CPU i386)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(LINUX_CPU arm64)
  else()
    set(LINUX_CPU x86_64)
  endif()
  set(FIREBASE_SDK_LIBDIR "${FIREBASE_CPP_SDK_DIR}/libs/linux/${LINUX_CPU}/${LINUX_ABI}")
endif()

# Defines a Firebase target, linking it with the correct prebuilt library.
function(add_firebase_target TARGET_NAME)
  if(NOT TARGET ${TARGET_NAME})
    if(MSVC)
      set(LIBRARY_NAME "${TARGET_NAME}.lib")
    else()
      set(LIBRARY_NAME "lib${TARGET_NAME}.a")
    endif()
    add_library(${TARGET_NAME} STATIC IMPORTED GLOBAL)
    set_target_properties(${TARGET_NAME} PROPERTIES
      IMPORTED_LOCATION "${FIREBASE_SDK_LIBDIR}/${LIBRARY_NAME}"
      INTERFACE_INCLUDE_DIRECTORIES "${FIREBASE_CPP_SDK_DIR}/include"
    )
    if(MSVC)
      set_target_properties(${TARGET_NAME} PROPERTIES
        IMPORTED_LOCATION_DEBUG "${FIREBASE_CPP_SDK_DIR}/libs/windows/${MSVC_VS_VERSION}/${MSVC_RUNTIME_MODE}/${MSVC_CPU}/Debug/${LIBRARY_NAME}"
        IMPORTED_LOCATION_RELEASE "${FIREBASE_CPP_SDK_DIR}/libs/windows/${MSVC_VS_VERSION}/${MSVC_RUNTIME_MODE}/${MSVC_CPU}/Release/${LIBRARY_NAME}"
      )
    endif()
    if(DISABLE_CXX11)
      set_target_properties(${TARGET_NAME} PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "_GLIBCXX_USE_CXX11_ABI=0"
      )
    endif()
  endif()
endfunction()

# Define targets for all the Firebase libraries
add_firebase_target(firebase_app)
add_firebase_target(firebase_ai)
add_firebase_target(firebase_analytics)
add_firebase_target(firebase_app_check)
add_firebase_target(firebase_auth)
add_firebase_target(firebase_database)
add_firebase_target(firebase_firestore)
add_firebase_target(firebase_functions)
add_firebase_target(firebase_installations)
add_firebase_target(firebase_messaging)
add_firebase_target(firebase_performance)
add_firebase_target(firebase_remote_config)
add_firebase_target(firebase_storage)
add_firebase_target(firebase_ump)

# Auth, Installations, and Remote Config on Linux desktop have an additional
# dependency on libsecret (via firebase_app's user_secure_manager), which needs
# to be added. If it cannot be found, we don't want to error, since the user
# might not be using these libraries, and the user will get a linker error if
# it is needed.
if(NOT APPLE AND NOT ANDROID AND UNIX)
  pkg_check_modules(LIBSECRET libsecret-1)

  if(NOT LIBSECRET_FOUND)
    message(WARNING "Unable to find libsecret, which is needed by Linux \
                     desktop implementations of App, Auth, Installations, and \
                     Remote Config. \
                     It can be installed on supported systems via: \
                     apt-get install libsecret-1-dev")
  else()
    if(LIBSECRET_LINK_LIBRARIES)
      set(_firebase_libsecret_libs "${LIBSECRET_LINK_LIBRARIES}")
    else()
      set(_firebase_libsecret_libs "${LIBSECRET_LIBRARIES}")
    endif()
    foreach(_libsecret_target
            firebase_app
            firebase_auth
            firebase_installations
            firebase_remote_config)
      if(TARGET ${_libsecret_target})
        set_property(TARGET ${_libsecret_target} APPEND PROPERTY
          INTERFACE_LINK_LIBRARIES "${_firebase_libsecret_libs}"
        )
      endif()
    endforeach()
    unset(_firebase_libsecret_libs)
  endif()
endif()
