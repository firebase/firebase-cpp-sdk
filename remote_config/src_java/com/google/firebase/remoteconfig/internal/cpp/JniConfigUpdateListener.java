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

package com.google.firebase.remoteconfig.internal.cpp;

import androidx.annotation.NonNull;
import com.google.firebase.remoteconfig.ConfigUpdate;
import com.google.firebase.remoteconfig.ConfigUpdateListener;
import com.google.firebase.remoteconfig.FirebaseRemoteConfigException;

/**
 * A ConfigUpdateListener that notifies C++ of config updates.
 */
public class JniConfigUpdateListener implements ConfigUpdateListener {
  // A C++ pointer to a ConfigUpdate listener function
  private long cListener;

  JniConfigUpdateListener(long cListener) {
    this.cListener = cListener;
  }

  public void onUpdate(@NonNull ConfigUpdate configUpdate) {
    nativeOnUpdate(cListener, configUpdate);
  }

  public void onError(@NonNull FirebaseRemoteConfigException error) {
    nativeOnError(cListener, error.getCode().value());
  }

  /**
   * These functions are implemented in the RemoteConfig C++ library.
   */
  private native void nativeOnUpdate(long cListener, ConfigUpdate configUpdate);

  private native void nativeOnError(long cListener, int error);
}
