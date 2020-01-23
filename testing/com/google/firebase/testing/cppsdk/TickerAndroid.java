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
