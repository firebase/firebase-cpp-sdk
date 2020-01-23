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
