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

"""Used to generate the build_type header used in the C++ build.

Generates the build_type header file with the given build type definition.
"""

from absl import app
from absl import flags
from absl import logging

FLAGS = flags.FLAGS

flags.DEFINE_string("output_file", None, "Location of the output file")
flags.DEFINE_string("build_type", "FIREBASE_CPP_BUILD_TYPE_RELEASED",
                    "The build type definition to generate with.")

BUILD_TYPE_TEMPLATE = """\
// Copyright 2017 Google Inc. All Rights Reserved.

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_BUILD_TYPE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_BUILD_TYPE_H_

// Available build configurations for the suite of libraries.
#define FIREBASE_CPP_BUILD_TYPE_HEAD 0
#define FIREBASE_CPP_BUILD_TYPE_STABLE 1
#define FIREBASE_CPP_BUILD_TYPE_RELEASED 2

// Currently selected build type.
#define FIREBASE_CPP_BUILD_TYPE {build_type}

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_BUILD_TYPE_H_
"""


def generate_header(build_type):
  """Return a C/C++ header for the given build type.

  Args:
    build_type: The build type definition to use.

  Returns:
    A string containing the C/C++ header file.
  """
  return BUILD_TYPE_TEMPLATE.format(build_type=build_type)


def main(unused_argv):
  output_file = FLAGS.output_file
  if not output_file:
    output_file = "build_type_generated.h"
    logging.debug("Using default --output_file='%s'", output_file)

  with open(output_file, "w") as outfile:
    outfile.write(generate_header(FLAGS.build_type))


if __name__ == "__main__":
  app.run(main)
