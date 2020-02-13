// Copyright 2020 Google LLC
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

package com.google.firebase.testing.cppsdk;

import java.util.Vector;

/** Manages all callbacks in tests by ticker. */
public final class TickerAndroid {
  /** The mimic time in integer. */
  private static long ticker = 0;

  /** The list of observers to notify when time elapses. */
  private static final Vector<TickerObserver> observers = new Vector<>();

  public static void register(TickerObserver observer) {
    observers.add(observer);
    // This mimics the case that an update happens immediately.
    observer.elapse();
  }

  public static void reset() {
    ticker = 0;
    observers.clear();
  }

  public static void elapse() {
    ++ticker;
    for (TickerObserver observer : observers) {
      observer.elapse();
    }
  }

  public static long now() {
    return ticker;
  }
}
