// Copyright 2017 Google LLC
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

package com.google.firebase.messaging;

import java.util.Map;

/**
 * Fake RemoteMessage class.
 */
public class RemoteMessage {

  public final String from;
  public final String to;
  public final Map<String, String> data;
  public final Integer ttl;
  public final String messageId;

  private RemoteMessage(
      String from, String to, Map<String, String> data, Integer ttl, String messageId) {
    this.from = from;
    this.to = to;
    this.data = data;
    this.ttl = ttl;
    this.messageId = messageId;
  }

  /**
   * Fake Builder class.
   */
  public static class Builder {

    private final String to;
    private Map<String, String> data;
    private Integer ttl;
    private String messageId;
    private String from;

    public Builder(String to) {
      this.to = to;
    }

    public Builder setData(Map<String, String> data) {
      this.data = data;
      return this;
    }

    public Builder setTtl(int ttl) {
      this.ttl = ttl;
      return this;
    }

    public Builder setMessageId(String messageId) {
      this.messageId = messageId;
      return this;
    }

    public RemoteMessage build() {
      return new RemoteMessage("my_from", to, data, ttl, messageId);
    }
  }
}
