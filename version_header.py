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

"""Used to generate the version header file used in the C++ build.

Generates the version header file with the given build type.
"""

import json
from absl import app
from absl import flags
from absl import logging

FLAGS = flags.FLAGS

flags.DEFINE_string(
    "input_file", "cpp_sdk_version.json",
    "Location of JSON file defining the versions for each build type")
flags.DEFINE_string("output_file", "version.h", "Location of the output file")
flags.DEFINE_string("build_type", "released",
                    "The build type to generate the header for")

VERSION_HEADER_TEMPLATE = r"""// Copyright 2016 Google Inc. All Rights Reserved.

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_VERSION_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_VERSION_H_

/// @def FIREBASE_VERSION_MAJOR
/// @brief Major version number of the Firebase C++ SDK.
/// @see kFirebaseVersionString
#define FIREBASE_VERSION_MAJOR {internal_major}
/// @def FIREBASE_VERSION_MINOR
/// @brief Minor version number of the Firebase C++ SDK.
/// @see kFirebaseVersionString
#define FIREBASE_VERSION_MINOR {internal_minor}
/// @def FIREBASE_VERSION_REVISION
/// @brief Revision number of the Firebase C++ SDK.
/// @see kFirebaseVersionString
#define FIREBASE_VERSION_REVISION {internal_revision}

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
namespace firebase {{

/// @brief String which identifies the current version of the Firebase C++
/// SDK.
///
/// @see FIREBASE_VERSION_MAJOR
/// @see FIREBASE_VERSION_MINOR
/// @see FIREBASE_VERSION_REVISION
static const char* kFirebaseVersionString = FIREBASE_VERSION_STRING;

}}  // namespace firebase
#endif  // !defined(DOXYGEN)

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_VERSION_H_
"""


class Error(Exception):
  """Exception raised by methods in this module."""
  pass


def generate_header(major, minor, revision):
  """Return a C/C++ header for the given version.

  Args:
    major: The major version to use.
    minor: The minor version to use.
    revision: The revision version to use.

  Returns:
    A string containing the C/C++ header file.
  """
  return VERSION_HEADER_TEMPLATE.format(
      internal_major=major, internal_minor=minor, internal_revision=revision)


def main(unused_argv):
  # Log that the default values for input and output are being used, as that
  # is the most likely cause of problems if there are any.
  if FLAGS["input_file"].using_default_value:
    logging.debug("Using default --input_file='%s'", FLAGS.input_file)
  if FLAGS["output_file"].using_default_value:
    logging.debug("Using default --output_file='%s'", FLAGS.output_file)

  with open(FLAGS.input_file, "r") as infile:
    cpp_sdk_versions = json.load(infile)
  cpp_sdk_version = cpp_sdk_versions[FLAGS.build_type]
  if not cpp_sdk_version:
    raise Error(
        "Unable to find version for build type (%s)." % FLAGS.build_type)

  # Get the major, minor, and revision values from the version.
  versions = cpp_sdk_version.split(".")
  if len(versions) != 3:
    raise Error("Failed to parse the values from the sdk version (%s)." %
                cpp_sdk_version)

  with open(FLAGS.output_file, "w") as outfile:
    outfile.write(generate_header(versions[0], versions[1], versions[2]))


if __name__ == "__main__":
  app.run(main)
