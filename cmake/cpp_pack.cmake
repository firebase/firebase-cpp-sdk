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

# Handles creation of the C++ package zip using cpack

# All functions do nothing unless FIREBASE_CPP_BUILD_PACKAGE is enabled.
if(${FIREBASE_CPP_BUILD_PACKAGE})
  # Including CPack multiple times can cause it to break, so guard against that.
  if(DEFINED CPACK_GENERATOR)
    return()
  endif()

  # iOS will get the platform name of Darwin if we dont change it here
  if(IOS)
    set(CPACK_SYSTEM_NAME "iOS")
  endif()

  set(CPACK_GENERATOR "ZIP")
  if(NOT DEFINED FIREBASE_CPP_VERSION OR FIREBASE_CPP_VERSION STREQUAL "")
    # Parse the version number from the json file.
    file(STRINGS ${FIREBASE_CPP_SDK_ROOT_DIR}/cpp_sdk_version.json version_file)
    string(REGEX MATCH "[0-9\.]+" FIREBASE_CPP_VERSION "${version_file}")
  endif()
  set(CPACK_PACKAGE_VERSION "${FIREBASE_CPP_VERSION}")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0) # DO NOT pack the root directory.

  include(CPack)

  if(ANDROID)
    set(FIREBASE_LIB_CONFIG_PATH "android")
  elseif(IOS)
    set(FIREBASE_LIB_CONFIG_PATH "ios")
  elseif(MSVC)
    set(FIREBASE_LIB_CONFIG_PATH "windows")
  elseif(APPLE)
    # Use the provided architecture, or fallback to a reasonable default.
    if(${CMAKE_OSX_ARCHITECTURES} STREQUAL "")
      set(FIREBASE_LIB_CONFIG_PATH "darwin/x86_64")
    else()
      set(FIREBASE_LIB_CONFIG_PATH "darwin/${CMAKE_OSX_ARCHITECTURES}")
    endif()
  else()
    # Check if building for 32 bit
    if("${CMAKE_CXX_FLAGS}" MATCHES "-m32")
      set(FIREBASE_LIB_CONFIG_PATH "linux/i386")
    else()
      set(FIREBASE_LIB_CONFIG_PATH "linux/x86_64")
    endif()
  endif()
endif()

# Packs the given target library into the libs folder, under the correct config
# and given subdirectory.
function(cpp_pack_library TARGET SUBDIRECTORY)
  if(NOT ${FIREBASE_CPP_BUILD_PACKAGE})
    return()
  endif()

  install(
    TARGETS "${TARGET}"
    ARCHIVE DESTINATION "libs/${FIREBASE_LIB_CONFIG_PATH}/${SUBDIRECTORY}"
  )
endfunction()

# Packs the given file into the libs folder, under the correct config
# and given subdirectory.
function(cpp_pack_library_file FILE_TO_PACK SUBDIRECTORY)
  cpp_pack_file(${FILE_TO_PACK}
                "libs/${FIREBASE_LIB_CONFIG_PATH}/${SUBDIRECTORY}")
endfunction()

# Packs the given file into the given destination.
function(cpp_pack_file FILE_TO_PACK DESTINATION)
  if(NOT ${FIREBASE_CPP_BUILD_PACKAGE})
    return()
  endif()

  # Get the real file path, for cases like symlinks.
  get_filename_component(path_to_file ${FILE_TO_PACK} REALPATH)

  install(
    FILES ${path_to_file}
    DESTINATION ${DESTINATION}
  )
endfunction()

# Packs the given directory into the given destination.
function(cpp_pack_dir DIR_TO_PACK DESTINATION)
  if(NOT ${FIREBASE_CPP_BUILD_PACKAGE})
    return()
  endif()

  install(
    DIRECTORY ${DIR_TO_PACK}
    DESTINATION ${DESTINATION}
  )
endfunction()

# Packs the files located in the calling CMake file's src/include directory.
function(cpp_pack_public_headers)
  cpp_pack_dir("${CMAKE_CURRENT_LIST_DIR}/src/include" .)
endfunction()