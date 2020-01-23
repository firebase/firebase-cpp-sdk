package com.google.firebase.testing.cppsdk;

/**
 * A generic listener to make the fake code compact. It is OnSucessListener, a OnFailureListener,
 * and a ResultCallback all-in-one. Please feel free to add more no-op on-method. Subclasses only
 * have to override the relevant method.
 */
public class FakeListener<R> {
  public void onSuccess(R result) {}

  public void onFailure(Exception e) {}

  public void onResult(R result) {}
}
