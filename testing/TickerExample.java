// Copyright 2020 Google
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

/** An example subclass of {@link TickerObserver} for unit-testing the ticker mechanism. */
public final class TickerExample implements TickerObserver {
  // When the callback should happen.
  private final long delay;

  public TickerExample(long delay) {
    this.delay = delay;
    TickerAndroid.register(this);
  }

  @Override
  public void elapse() {
    // We pass in ticker directly to save a few JNI calls. A fake should implement the logic right
    // here without using native function. We use native function here in order to consolidate logic
    // in the unit test .cc file.
    nativeFunction(TickerAndroid.now(), delay);
  }

  private native void nativeFunction(long ticker, long delay);
}
