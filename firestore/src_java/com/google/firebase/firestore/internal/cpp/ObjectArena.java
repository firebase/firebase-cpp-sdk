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

import java.util.concurrent.ConcurrentHashMap;

/**
 * A companion class used by the C++ class
 * {@code firebase::firestore::jni::ArenaRef} to map {@code long} primitive
 * values to objects.
 *
 * This class cannot be instantiated; all uses must be done through the static
 * methods. This effectively serves as a global singleton map that exists for
 * the entire lifetime of the Java application.
 *
 * Technically, the logic in this class could have been written entirely in C++,
 * using JNI to create the map and call its methods; however, this
 * proved to be slow due to the overhead of JNI calls. By instead
 * writing the class in Java, this improves the performance compared
 * to the equivalent JNI calls.
 */
public final class ObjectArena {
  private static final ConcurrentHashMap<Long, Object> map =
      new ConcurrentHashMap<>();

  private ObjectArena() {
    throw new UnsupportedOperationException();
  }

  public static void set(long id, Object object) {
    if (object == null) {
      map.remove(id);
    } else {
      map.put(id, object);
    }
  }

  public static Object get(long id) {
    return map.get(id);
  }

  public static void remove(long id) {
    map.remove(id);
  }
}
