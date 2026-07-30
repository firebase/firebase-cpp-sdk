// Copyright 2024 Google LLC
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

package com.google.firebase.remoteconfig;

import com.google.firebase.testing.cppsdk.FakeReporter;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;

/** Fake CustomSignals */
public class CustomSignals {
  final Map<String, String> customSignals;

  CustomSignals(Builder builder) {
    this.customSignals = builder.customSignals;
  }

  @Override
  public String toString() {
    Map<String, String> sorted = new TreeMap<>(customSignals);
    return sorted.toString();
  }

  /** Fake Builder */
  public static class Builder {
    private final Map<String, String> customSignals = new HashMap<>();

    public Builder put(String key, String value) {
      customSignals.put(key, value);
      return this;
    }

    public Builder put(String key, long value) {
      customSignals.put(key, Long.toString(value));
      return this;
    }

    public Builder put(String key, double value) {
      customSignals.put(key, Double.toString(value));
      return this;
    }

    public CustomSignals build() {
      FakeReporter.addReport("CustomSignals.Builder.build");
      return new CustomSignals(this);
    }
  }
}
