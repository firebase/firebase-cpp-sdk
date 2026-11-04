// Copyright 2020 Google LLC
#include "firestore/src/common/compiler_info.h"

#include <ostream>
#include <sstream>
#include "firestore/src/common/macros.h"

namespace firebase::firestore {
namespace {

struct CompilerId {};
std::ostream& operator<<(std::ostream& os, CompilerId) {
#if defined(__apple_build_version__) && defined(__clang__)
  return os << "AppleClang";
#elif defined(__clang__)
  return os << "Clang";
#elif defined(__GNUC__)
  return os << "GNU";
#elif defined(_MSC_VER)
  return os << "MSVC";
#else
  return os << "Unknown";
#endif
}

struct CompilerVersion {};
std::ostream& operator<<(std::ostream& os, CompilerVersion) {
#if defined(__apple_build_version__) && defined(__clang__)
  return os << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__ << "."
            << __apple_build_version__;
#elif defined(__clang__)
  return os << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
#elif defined(__GNUC__)
  return os << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
#elif defined(_MSC_VER)
  os << _MSC_VER / 100 << "." << _MSC_VER % 100;
#if defined(_MSC_FULL_VER)
  os << "." << (_MSC_VER >= 1400 ? _MSC_FULL_VER % 100000 : _MSC_FULL_VER % 10000);
#endif
  return os;
#else
  return os << "Unknown";
#endif
}

struct CompilerFeatures {};
std::ostream& operator<<(std::ostream& os, CompilerFeatures) {
#if FIRESTORE_HAVE_EXCEPTIONS
  return os << "ex";
#else
  return os << "noex";
#endif
}

#ifdef _MSC_VER
#define FIRESTORE__CPLUSPLUS _MSVC_LANG
#else
#define FIRESTORE__CPLUSPLUS __cplusplus
#endif

struct LanguageVersion {};
std::ostream& operator<<(std::ostream& os, LanguageVersion) {
  switch (FIRESTORE__CPLUSPLUS) {
    case 199711L: return os << "1998";
    case 201103L: return os << "2011";
    case 201402L: return os << "2014";
    case 201703L: return os << "2017";
    case 202002L: return os << "2020";
    case 202302L: return os << "2023";
    default: return os << (FIRESTORE__CPLUSPLUS > 202002L ? "latest" : "unknown");
  }
}

struct StandardLibraryVendor {};
std::ostream& operator<<(std::ostream& os, StandardLibraryVendor) {
#if defined(_STLPORT_VERSION)
  return os << "stlport";
#elif defined(__GLIBCXX__) || defined(__GLIBCPP__)
  return os << "gnustl";
#elif defined(_LIBCPP_VERSION) || defined(_LIBCPP_STD_VER)
  return os << "libcpp";
#elif defined(_MSC_VER)
  return os << "msvc";
#else
  return os << "unknown";
#endif
}

}  // namespace

std::string GetFullCompilerInfo() {
  std::ostringstream os;
  os << CompilerId() << "-" << CompilerVersion() << "-" << CompilerFeatures() << "-"
     << LanguageVersion() << "-" << StandardLibraryVendor();
  return os.str();
}

}  // namespace firebase::firestore
