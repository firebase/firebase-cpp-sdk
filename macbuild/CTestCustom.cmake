# Copyright 2019 Google
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

# Define the tests that need to be ignored.
set(CTEST_CUSTOM_TESTS_IGNORE
    
    # Tests from libuv, that can't be disabled normally.
    uv_test
    uv_test_a
    # Tests from zlib, that can't be disabled normally.
    example
    example64
    # Disabling specific firestore tests
    # objc_spec test doesn't fail locally but fails consistently on 
    # github runners
    firestore_objc_spec_test
    # objc_integration test needs a simulator to be setup and just hangs
    # forever without one.
    firestore_objc_integration_test
)
