# Copyright 2026 Google LLC
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

# Opt-in Clang Thread Safety Analysis (-Wthread-safety) for SDK targets.
# No-op on GCC, MSVC, or when FIREBASE_CPP_ENABLE_THREAD_SAFETY_ANALYSIS is OFF.

function(firebase_cpp_enable_thread_safety_analysis target)
  if(NOT FIREBASE_CPP_ENABLE_THREAD_SAFETY_ANALYSIS)
    return()
  endif()
  if(NOT TARGET ${target})
    return()
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${target} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:-Wthread-safety>
      $<$<COMPILE_LANGUAGE:CXX>:-Wno-error=thread-safety>
      $<$<COMPILE_LANGUAGE:OBJCXX>:-Wthread-safety>
      $<$<COMPILE_LANGUAGE:OBJCXX>:-Wno-error=thread-safety>
    )
  endif()
endfunction()
