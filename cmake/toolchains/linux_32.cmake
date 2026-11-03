# Copyright 2020 Google
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

# Toolchain file for building 32-bit Linux libraries

set(CMAKE_C_FLAGS_INIT "-m32")
set(CMAKE_CXX_FLAGS_INIT "-m32")
if(NOT "${CMAKE_C_FLAGS}" MATCHES "(^| )-m32( |$)")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
endif()
if(NOT "${CMAKE_CXX_FLAGS}" MATCHES "(^| )-m32( |$)")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()
set(CMAKE_LIBRARY_PATH "/usr/lib/i386-linux-gnu")
include_directories(SYSTEM "/usr/include/i386-linux-gnu")
if(NOT "$ENV{PKG_CONFIG_PATH}" MATCHES "/usr/lib/i386-linux-gnu/pkgconfig")
  if("$ENV{PKG_CONFIG_PATH}" STREQUAL "")
    set(ENV{PKG_CONFIG_PATH} "/usr/lib/i386-linux-gnu/pkgconfig")
  else()
    set(ENV{PKG_CONFIG_PATH} "/usr/lib/i386-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
  endif()
endif()
