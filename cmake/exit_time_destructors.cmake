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

# Makes Clang diagnose static and global C++ objects in `target` that need a
# destructor to run at process exit. Such SDK state can be destroyed before
# user-owned static Firebase objects that still use it; wrap it in
# firebase::NoDestructor (app/src/no_destructor.h) instead.
#
# The diagnostic is a warning by default, and an error when
# FIREBASE_CPP_WERROR_EXIT_TIME_DESTRUCTORS is ON, so that a declaration in a
# third-party header cannot break builds of the SDK from source.
#
# Only Clang and AppleClang implement -Wexit-time-destructors. Builds with
# other compilers are covered by scripts/gha/check_exit_time_destructors.py.
# The flag is restricted to C++ so C and Swift sources do not receive it.
#
# Args:
#   target: The library target to check.
function(firebase_cpp_reject_exit_time_destructors target)
  if(FIREBASE_CPP_WERROR_EXIT_TIME_DESTRUCTORS)
    set(exit_time_destructors_flag "-Werror=exit-time-destructors")
  else()
    set(exit_time_destructors_flag "-Wexit-time-destructors")
  endif()
  target_compile_options(${target} PRIVATE
      $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang,AppleClang>>:${exit_time_destructors_flag}>)
endfunction()
