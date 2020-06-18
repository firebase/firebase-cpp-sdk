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

"""Tests for google3.firebase.app.client.cpp.build_type_header."""

from google3.testing.pybase import googletest
from google3.firebase.app.client.cpp import build_type_header

EXPECTED_BUILD_TYPE_HEADER = """\
// Copyright 2017 Google Inc. All Rights Reserved.

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_BUILD_TYPE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_BUILD_TYPE_H_

// Available build configurations for the suite of libraries.
#define FIREBASE_CPP_BUILD_TYPE_HEAD 0
#define FIREBASE_CPP_BUILD_TYPE_STABLE 1
#define FIREBASE_CPP_BUILD_TYPE_RELEASED 2

// Currently selected build type.
#define FIREBASE_CPP_BUILD_TYPE TEST_BUILD_TYPE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_BUILD_TYPE_H_
"""


class BuildTypeHeaderTest(googletest.TestCase):

  def test_build_type_header(self):
    result_header = build_type_header.generate_header('TEST_BUILD_TYPE')
    self.assertEqual(result_header, EXPECTED_BUILD_TYPE_HEADER)


if __name__ == '__main__':
  googletest.main()
