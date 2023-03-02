/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.firebase.appcheck.internal.cpp;

import androidx.annotation.NonNull;
import com.google.firebase.appcheck.AppCheckToken;

/**
 * A trivial implementation of the abstract class AppCheckToken
 */
public class JniAppCheckToken extends AppCheckToken {
  private String token;
  private long expiration;

  JniAppCheckToken(String token, long expiration) {
    this.token = token;
    this.expiration = expiration;
  }

  @NonNull
  @Override
  public String getToken() {
    return token;
  }

  @Override
  public long getExpireTimeMillis() {
    return expiration;
  }
}
