// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "firestore/src/common/compiler_info.h"

#include <ostream>
#include <sstream>

#include "app/meta/move.h"
#include "firestore/src/common/macros.h"

namespace firebase {
namespace firestore {

namespace {

// The code in this file is adapted from
// https://github.com/googleapis/google-cloud-cpp/blob/7f418183fc4f07cd76995bd12f7c1971b8e057ac/google/cloud/internal/compiler_info.cc
// and
// https://github.com/googleapis/google-cloud-cpp/blob/7f418183fc4f07cd76995bd12f7c1971b8e057ac/google/cloud/internal/port_platform.h.

/**
 * Returns the compiler ID.
 *
 * The Compiler ID is a string like "GNU" or "Clang", as described by
 * https://cmake.org/cmake/help/v3.5/variable/CMAKE_LANG_COMPILER_ID.html
 */
struct CompilerId {};

std::ostream& operator<<(std::ostream& os, CompilerId) {
  // The macros for determining the compiler ID are taken from:
  // https://gitlab.kitware.com/cmake/cmake/tree/v3.5.0/Modules/Compiler/\*-DetermineCompiler.cmake
  // We do not care to detect every single compiler possible and only target
  // the most popular ones.
  //
  // Order is significant as some compilers can define the same macros.

#if defined(__apple_build_version__) && defined(__clang__)
  os << "AppleClang";
#elif defined(__clang__)
  os << "Clang";
#elif defined(__GNUC__)
  os << "GNU";
#elif defined(_MSC_VER)
  os << "MSVC";
#else
  os << "Unknown";
#endif
  return os;
}

/** Returns the compiler version. This string will be something like "9.1.1". */
struct CompilerVersion {};

std::ostream& operator<<(std::ostream& os, CompilerVersion) {
#if defined(__apple_build_version__) && defined(__clang__)
  os << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__
     << "." << __apple_build_version__;

#elif defined(__clang__)
  os << __clang_major__ << "." << __clang_minor__ << "."
     << __clang_patchlevel__;

#elif defined(__GNUC__)
  os << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;

#elif defined(_MSC_VER)
  os << _MSC_VER / 100 << ".";
  os << _MSC_VER % 100;
#if defined(_MSC_FULL_VER)
#if _MSC_VER >= 1400
  os << "." << _MSC_FULL_VER % 100000;
#else
  os << "." << _MSC_FULL_VER % 10000;
#endif  // _MSC_VER >= 1400
#endif  // defined(_MSC_VER)

#else
  os << "Unknown";

#endif  // defined(__apple_build_version__) && defined(__clang__)

  return os;
}

/**
 * Returns certain interesting compiler features.
 *
 * Currently this returns one of "ex" or "noex" to indicate whether or not
 * C++ exceptions are enabled.
 */
struct CompilerFeatures {};

std::ostream& operator<<(std::ostream& os, CompilerFeatures) {
#if FIRESTORE_HAVE_EXCEPTIONS
  os << "ex";
#else
  os << "noex";
#endif  // FIRESTORE_HAVE_EXCEPTIONS
  return os;
}

// Microsoft Visual Studio does not define `__cplusplus` correctly by default:
// https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus
// Instead, `_MSVC_LANG` macro can be used which uses the same version numbers
// as the standard `__cplusplus` macro (except when the `/std:c++latest` option
// is used, in which case it will be higher).
#ifdef _MSC_VER
#define FIRESTORE__CPLUSPLUS _MSVC_LANG
#else
#define FIRESTORE__CPLUSPLUS __cplusplus
#endif  // _MSC_VER

/** Returns the 4-digit year of the C++ language standard. */
struct LanguageVersion {};

std::ostream& operator<<(std::ostream& os, LanguageVersion) {
  switch (FIRESTORE__CPLUSPLUS) {
    case 199711L:
      os << "1998";
      break;
    case 201103L:
      os << "2011";
      break;
    case 201402L:
      os << "2014";
      break;
    case 201703L:
      os << "2017";
      break;
    case 202002L:
      os << "2020";
      break;
    default:
#ifdef _MSC_VER
      // According to
      // https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros,
      // _MSVC_LANG is "set to a higher, unspecified value when the
      // `/std:c++latest` option is specified".
      if (FIRESTORE__CPLUSPLUS > 202002L) {
        os << "latest";
        break;
      }
#endif  // _MSC_VER
      os << "unknown";
      break;
  }

  return os;
}

struct StandardLibraryVendor {};

std::ostream& operator<<(std::ostream& os, StandardLibraryVendor) {
#if defined(_STLPORT_VERSION)
  os << "stlport";
#elif defined(__GLIBCXX__) || defined(__GLIBCPP__)
  os << "gnustl";
#elif defined(_LIBCPP_STD_VER)
  os << "libcpp";
#elif defined(_MSC_VER)
  os << "msvc";
#else
  os << "unknown";
#endif
  return os;
}

}  // namespace

std::string GetFullCompilerInfo() {
  std::ostringstream os;
  os << CompilerId() << "-" << CompilerVersion() << "-" << CompilerFeatures()
     << "-" << LanguageVersion() << "-" << StandardLibraryVendor();
  return Move(os).str();
}

}  // namespace firestore
}  // namespace firebase
