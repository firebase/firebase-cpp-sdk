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
 * A companion class used by the C++ class firebase::firestore::jni::ArenaRef to map long primitive values to Objects.
 *
 * Technically, this could have been written using JNI calling HashMap methods directly; however, that is extremely slow
 * and sidesteps an amazing optimization done by Android's ART runtime called "Value Type Profiling".
 *
 * Below is an explanation of Value Type Profiling from a now-deleted blog post that used to be posted at
 * https://android-developers.googleblog.com/2015/06/android-m-preview-2-getting-ready-for.html.
 *
 * Another key area of focus has been around reducing the memory footprint of Android apps. To help with this, we've
 * introduced a new feature called "value type profiling" in the Android Runtime (ART). In Java, the standard data types
 * (int, float, boolean, etc.) are primitives, which means that they're stored on the stack and can be accessed quickly
 * by the JVM. However, Java also provides "boxed" versions of these types (Integer, Float, Boolean, etc.) which are
 * objects and are stored on the heap. Boxed types are less efficient than primitives because they require extra memory
 * allocation and deallocation, and can also cause performance issues when used in tight loops.
 *
 * To help address these issues, ART now performs value type profiling to determine when a boxed type can be replaced
 * with its corresponding primitive type. This optimization can lead to significant performance improvements in certain
 * cases.
 *
 * For example, consider the following code:
 *
 * List<Long> list = new ArrayList<Long>();
 * for (int i = 0; i < 1000000; i++) {
 *   list.add((long)i);
 * }
 * long sum = 0;
 * for (Long value : list) {
 *   sum += value;
 * }
 *
 * In this code, we're creating a list of boxed Long values, and then iterating over the list and summing up the values.
 * Because we're using boxed types, this code is less efficient than it could be. However, with value type profiling,
 * ART can determine that the list contains only Long values, and can replace the boxed Long values with primitive long
 * values. This leads to a significant performance improvement.
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
