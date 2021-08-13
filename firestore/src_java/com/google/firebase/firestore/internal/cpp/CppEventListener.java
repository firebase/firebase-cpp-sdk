/*
 * Copyright 2021 Google LLC
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

/** Implements Firestore EventListener base class. */
public class CppEventListener {
  protected long cppFirestoreObject = 0;
  protected long cppListenerObject = 0;

  /** Construct a CppEventListener. Parameters are C++ pointers. */
  public CppEventListener(long cppFirestoreObject, long cppListenerObject) {
    this.cppFirestoreObject = cppFirestoreObject;
    this.cppListenerObject = cppListenerObject;
  }

  /** Discard native pointers from this instance. */
  public synchronized void discardPointers() {
    this.cppFirestoreObject = 0;
    this.cppListenerObject = 0;
  }
}
