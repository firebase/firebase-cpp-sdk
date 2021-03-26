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

"""Tests for google3.firebase.app.client.cpp.binary_to_array."""

from google3.testing.pybase import googletest
from google3.firebase.app.client.cpp import binary_to_array

EXPECTED_SOURCE_FILE = """\
// Copyright 2016 Google Inc. All Rights Reserved.

#include <stdlib.h>

namespace test_outer_namespace {
namespace test_inner_namespace {

extern const size_t test_array_name_size;
extern const char test_fileid[];
extern const unsigned char test_array_name[];

const unsigned char test_array_name[] = {
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00  // Extra \\0 to make it a C string
};

const size_t test_array_name_size =
  sizeof(test_array_name) - 1;

const char test_fileid[] =
  "test_filename";

}  // namespace test_inner_namespace
}  // namespace test_outer_namespace
"""

EXPECTED_HEADER_FILE = """\
// Copyright 2016 Google Inc. All Rights Reserved.

#ifndef TEST_HEADER_GUARD
#define TEST_HEADER_GUARD

#include <stdlib.h>

namespace test_outer_namespace {
namespace test_inner_namespace {

extern const size_t test_array_name_size;
extern const unsigned char test_array_name[];
extern const char test_fileid[];

}  // namespace test_inner_namespace
}  // namespace test_outer_namespace

#endif  // TEST_HEADER_GUARD
"""

namespaces = ["test_outer_namespace", "test_inner_namespace"]
array_name = "test_array_name"
array_size_name = "test_array_name_size"
fileid = "test_fileid"
filename = "test_filename"
input_bytes = [1, 2, 3, 4, 5, 6, 7]
header_guard = "TEST_HEADER_GUARD"


class BinaryToArrayTest(googletest.TestCase):

  def test_source_file(self):
    result_source = binary_to_array.source(
        namespaces, array_name, array_size_name, fileid, filename, input_bytes)
    self.assertEqual("\n".join(result_source), EXPECTED_SOURCE_FILE)

  def test_header_file(self):
    result_header = binary_to_array.header(header_guard, namespaces, array_name,
                                           array_size_name, fileid)
    self.assertEqual("\n".join(result_header), EXPECTED_HEADER_FILE)


if __name__ == "__main__":
  googletest.main()
