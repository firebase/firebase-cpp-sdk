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
 * A companion class used by the C++ class
 * {@code firebase::firestore::jni::ArenaRef} to map {@code long} primitive
 * values to objects.
 *
 * This class cannot be instantiated; all uses must be done through the static
 * methods. This effectively serves as a global singleton map that exists for
 * the entire lifetime of the Java application.
 *
 * Technically, the logic in this class could have been written entirely in C++,
 * using JNI to create the {@code HashMap} and call its methods; however, this
 * proved to be slow, mostly due to the boxing and unboxing of the {@code long}
 * primitive values to and from {@code Long} objects, respectively. By instead
 * writing the class in Java, Android's ART runtime elides the boxing and
 * unboxing since it can prove to itself that the code does not rely on object
 * identity of the {@code Long} objects. This improves the performance by an
 * order of magnitude compared the the equivalent JNI calls, which cannot have
 * the boxing and unboxing optimized away.
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
