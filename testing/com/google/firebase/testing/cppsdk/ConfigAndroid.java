// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.firebase.testing.cppsdk;

import java.nio.ByteBuffer;

/** Manages configurations of all fakes. */
public final class ConfigAndroid {
  // We keep this raw data in order to be able to return it back for merge.
  private static ByteBuffer testDataConfig;

  public static ConfigRow get(String fake) {
    if (testDataConfig == null) {
      throw new RuntimeException("No test data at all");
    }
    // The lookupByKey() does not work today, see https://github.com/google/flatbuffers/issues/4042.
    // The data passed in may not conform. So we just iterate over the test data.
    TestDataConfig config = TestDataConfig.getRootAsTestDataConfig(testDataConfig);
    for (int i = 0; i < config.configLength(); ++i) {
      ConfigRow row = config.config(i);
      if (row.fake().equals(fake)) {
        return row;
      }
    }
    return null;
  }

  public static void setImpl(byte[] testDataBinary) {
    if (testDataBinary == null) {
      testDataConfig = null;
    } else {
      testDataConfig = ByteBuffer.wrap(testDataBinary);
    }
  }
}
