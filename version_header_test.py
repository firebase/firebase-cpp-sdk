# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for google3.firebase.app.client.cpp.version_header."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from google3.testing.pybase import googletest
from google3.firebase.app.client.cpp import version_header

EXPECTED_VERSION_HEADER = r"""// Copyright 2016 Google Inc. All Rights Reserved.

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_VERSION_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_VERSION_H_

/// @def FIREBASE_VERSION_MAJOR
/// @brief Major version number of the Firebase C++ SDK.
/// @see kFirebaseVersionString
#define FIREBASE_VERSION_MAJOR 1
/// @def FIREBASE_VERSION_MINOR
/// @brief Minor version number of the Firebase C++ SDK.
/// @see kFirebaseVersionString
#define FIREBASE_VERSION_MINOR 2
/// @def FIREBASE_VERSION_REVISION
/// @brief Revision number of the Firebase C++ SDK.
/// @see kFirebaseVersionString
#define FIREBASE_VERSION_REVISION 3

/// @cond FIREBASE_APP_INTERNAL
#define FIREBASE_STRING_EXPAND(X) #X
#define FIREBASE_STRING(X) FIREBASE_STRING_EXPAND(X)
/// @endcond

// Version number.
// clang-format off
#define FIREBASE_VERSION_NUMBER_STRING        \
  FIREBASE_STRING(FIREBASE_VERSION_MAJOR) "." \
  FIREBASE_STRING(FIREBASE_VERSION_MINOR) "." \
  FIREBASE_STRING(FIREBASE_VERSION_REVISION)
// clang-format on

// Identifier for version string, e.g. kFirebaseVersionString.
#define FIREBASE_VERSION_IDENTIFIER(library) k##library##VersionString

// Concatenated version string, e.g. "Firebase C++ x.y.z".
#define FIREBASE_VERSION_STRING(library) \
  #library " C++ " FIREBASE_VERSION_NUMBER_STRING

#if !defined(DOXYGEN)
#if !defined(_WIN32) && !defined(__CYGWIN__)
#define DEFINE_FIREBASE_VERSION_STRING(library)          \
  extern volatile __attribute__((weak))                  \
      const char* FIREBASE_VERSION_IDENTIFIER(library);  \
  volatile __attribute__((weak))                         \
      const char* FIREBASE_VERSION_IDENTIFIER(library) = \
          FIREBASE_VERSION_STRING(library)
#else
#define DEFINE_FIREBASE_VERSION_STRING(library)             \
  static const char* FIREBASE_VERSION_IDENTIFIER(library) = \
      FIREBASE_VERSION_STRING(library)
#endif  // !defined(_WIN32) && !defined(__CYGWIN__)
#else   // if defined(DOXYGEN)

/// @brief Namespace that encompasses all Firebase APIs.
namespace firebase {

/// @brief String which identifies the current version of the Firebase C++
/// SDK.
///
/// @see FIREBASE_VERSION_MAJOR
/// @see FIREBASE_VERSION_MINOR
/// @see FIREBASE_VERSION_REVISION
static const char* kFirebaseVersionString = FIREBASE_VERSION_STRING;

}  // namespace firebase
#endif  // !defined(DOXYGEN)

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_VERSION_H_
"""


class VersionHeaderGeneratorTest(googletest.TestCase):

  def test_generate_header(self):
    result_header = version_header.generate_header(1, 2, 3)
    self.assertEqual(result_header, EXPECTED_VERSION_HEADER)


if __name__ == '__main__':
  googletest.main()
