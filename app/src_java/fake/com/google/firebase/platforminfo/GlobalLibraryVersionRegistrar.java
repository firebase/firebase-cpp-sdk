/*
 * Copyright 2019 Google LLC
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

package com.google.firebase.platforminfo;

import java.util.HashSet;
import java.util.Set;

/**
 * Fake
 * //j/c/g/a/gmscore/integ/client/firebase_common/src/com/google/firebase/platforminfo/GlobalLibraryVersionRegistrar.java
 */
public final class GlobalLibraryVersionRegistrar {
  public static GlobalLibraryVersionRegistrar getInstance() {
    return new GlobalLibraryVersionRegistrar();
  }

  public void registerVersion(String library, String version) {}

  public Set<Object> getRegisteredVersions() {
    return new HashSet<>();
  }
}
