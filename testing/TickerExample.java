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
