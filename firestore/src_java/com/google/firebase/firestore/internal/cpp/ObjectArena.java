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

package com.google.firebase.firestore.internal.cpp;

import java.util.HashMap;

/**
 * A companion class used by the C++ class firebase::firestore::jni::ArenaRef to
 * map long primitive values to Objects.
 *
 * Technically, this could have been written using JNI calling HashMap methods
 * directly; however, that is extremely slow and sidesteps an amazing
 * optimization done by Android's ART runtime that elides the boxing and
 * unboxing of the Long keys.
 */
public final class ObjectArena {
  private static final HashMap<Long, Object> map = new HashMap<>();

  private ObjectArena() {
    throw new UnsupportedOperationException();
  }

  public static synchronized void set(long id, Object object) {
    map.put(id, object);
  }

  public static synchronized Object get(long id) {
    return map.get(id);
  }

  public static synchronized void remove(long id) {
    map.remove(id);
  }
}
