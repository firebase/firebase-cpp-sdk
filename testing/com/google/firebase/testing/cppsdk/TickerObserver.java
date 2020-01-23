package com.google.firebase.testing.cppsdk;

/** Observes the ticker and handle callbacks. */
public interface TickerObserver {

  // After registered with TickerAndroid, this will be called per each ticker elapses.
  public void elapse();
}
