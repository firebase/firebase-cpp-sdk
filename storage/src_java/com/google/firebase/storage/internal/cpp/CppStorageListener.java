// Copyright 2016 Google LLC
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

package com.google.firebase.storage.internal.cpp;

import com.google.firebase.storage.OnPausedListener;
import com.google.firebase.storage.OnProgressListener;

/** Implements Cloud Storage OnPausedListener/OnProgressListener class. * */
public class CppStorageListener implements OnPausedListener<Object>, OnProgressListener<Object> {
  protected long cppStorageObject = 0;
  protected long cppListenerObject = 0;
  protected final Object lockObject = new Object();

  /** Construct a CppStorageListener. Parameters are C++ pointers. */
  public CppStorageListener(long cppStorageObject, long cppListenerObject) {
    this.cppStorageObject = cppStorageObject;
    this.cppListenerObject = cppListenerObject;
  }

  /** Discard native pointers from this instance. */
  public void discardPointers() {
    synchronized (lockObject) {
      this.cppStorageObject = 0;
      this.cppListenerObject = 0;
    }
  }

  @Override
  public void onPaused(Object snapshot) {
    synchronized (lockObject) {
      if (cppListenerObject != 0 && cppStorageObject != 0) {
        nativeCallback(cppStorageObject, cppListenerObject, snapshot, true);
      }
    }
  }

  @Override
  public void onProgress(Object snapshot) {
    synchronized (lockObject) {
      if (cppListenerObject != 0 && cppStorageObject != 0) {
        nativeCallback(cppStorageObject, cppListenerObject, snapshot, false);
      }
    }
  }

  private static native void nativeCallback(
      long storageObject, long listenerObject, Object snapshot, boolean onPaused);
}
