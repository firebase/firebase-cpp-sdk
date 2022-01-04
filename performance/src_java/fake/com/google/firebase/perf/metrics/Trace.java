package com.google.firebase.perf.metrics;

import android.support.annotation.Keep;
import android.util.Log;
import androidx.annotation.Nullable;
import com.google.firebase.perf.FirebasePerformanceAttributable;
import com.google.firebase.testing.cppsdk.FakeReporter;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;

/**
 * Trace allows you to set beginning and end of a certain action in your app. Fake implementation.
 */
public class Trace implements FirebasePerformanceAttributable {
  private final String name;
  private final Map<String, Long> counters;
  private final Map<String, String> attributes;
  private static final String LOG_TAG = "FirebasePerformanceAndroidFake";

  @Nullable private Date startTime = null;
  @Nullable private Date endTime = null;

  /**
   * Creates a Trace object with given name.
   *
   * @param name name of the trace object
   */
  public static Trace create(String name) {
    return new Trace(name);
  }

  private Trace(String name) {
    FakeReporter.addReport("new Trace", name);
    this.name = name;
    counters = new HashMap<>();
    attributes = new HashMap<>();
  }

  /** Starts this trace. */
  @Keep
  public void start() {
    FakeReporter.addReport("Trace.start");
    startTime = new Date();
  }

  /** Stops this trace. */
  @Keep
  public void stop() {
    FakeReporter.addReport("Trace.stop");
    if (!hasStarted()) {
      Log.e(LOG_TAG, String.format("Trace '%s' has not been started so unable to stop!", name));
      return;
    }
    if (isStopped()) {
      Log.e(LOG_TAG, String.format("Trace '%s' has already stopped, should not stop again!", name));
      return;
    }

    endTime = new Date();
    if (!name.isEmpty()) {
      Log.i(LOG_TAG, "Logged trace with name: " + name);
    } else {
      Log.e(LOG_TAG, "Trace name is empty, no log is sent to server");
    }
  }

  /**
   * Atomically increments the metric with the given name in this trace by the incrementBy value. If
   * the metric does not exist, a new one will be created. If the trace has not been started or has
   * already been stopped, returns immediately without taking action.
   *
   * @param metricName Name of the metric to be incremented. Requires no leading or trailing
   *     whitespace, no leading underscore [_] character, max length of 32 characters.
   * @param incrementBy Amount by which the metric has to be incremented. Note: The fake
   *     implementation is not thread safe.
   */
  @Keep
  public void incrementMetric(String metricName, long incrementBy) {
    FakeReporter.addReport("Trace.incrementMetric", metricName, Long.toString(incrementBy));
    Long currentValue = counters.get(metricName);
    currentValue = currentValue != null ? currentValue : 0;
    counters.put(metricName, currentValue + incrementBy);
  }

  /**
   * Gets the value of the metric with the given name in the current trace. If a metric with the
   * given name doesn't exist, it is NOT created and a 0 is returned. This method is atomic.
   *
   * @param metricName Name of the metric to get. Requires no leading or trailing whitespace, no
   *     leading underscore '_' character, max length is 32 characters.
   * @return Value of the metric or 0 if it hasn't yet been set.
   */
  @Keep
  public long getLongMetric(String metricName) {
    FakeReporter.addReport("Trace.getLongMetric", metricName);
    if (metricName != null) {
      Long metricValue = counters.get(metricName);
      metricValue = metricValue != null ? metricValue : 0;
      return metricValue;
    }

    return 0;
  }

  /**
   * Sets the value of the metric with the given name in this trace to the value provided. If a
   * metric with the given name doesn't exist, a new one will be created. If the trace has not been
   * started or has already been stopped, returns immediately without taking action. This method is
   * atomic.
   *
   * @param metricName Name of the metric to set. Requires no leading or trailing whitespace, no
   *     leading underscore '_' character, max length is 32 characters.
   * @param value The value to which the metric should be set to.
   */
  @Keep
  public void putMetric(String metricName, long value) {
    FakeReporter.addReport("Trace.putMetric", metricName, Long.toString(value));
    if (!hasStarted()) {
      Log.w(LOG_TAG,
          String.format("Cannot set value for metric '%s' for trace '%s' because it's not started",
              metricName, name));
      return;
    }
    if (isStopped()) {
      Log.w(LOG_TAG,
          String.format("Cannot set value for metric '%s' for trace '%s' because it's been stopped",
              metricName, name));
      return;
    }

    if (metricName != null) {
      counters.put(metricName, value);
    }
  }

  /** Log a message if Trace is not stopped when finalize() is called. */
  @Override
  protected void finalize() throws Throwable {
    try {
      // If trace is started but not stopped when it reaches finalize(), log a warning msg.
      if (isActive()) {
        Log.w(LOG_TAG,
            String.format("Trace '%s' is started but not stopped when it is destructed!", name));
      }
    } finally {
      super.finalize();
    }
  }

  /**
   * non-zero endTime indicates Trace's stop() method has been called already.
   *
   * @return true if trace is stopped. false if not stopped.
   */
  private boolean isStopped() {
    return endTime != null;
  }

  /**
   * non-zero startTime indicates that Trace's start() method has been called
   *
   * @return true if trace has started, false if it has not started
   */
  private boolean hasStarted() {
    return startTime != null;
  }

  /**
   * Returns whether the trace is active.
   *
   * @return true if trace has been started but not stopped.
   */
  private boolean isActive() {
    return hasStarted() && !isStopped();
  }

  /**
   * Sets a String value for the specified attribute. Updates the value of the attribute if the
   * attribute already exists. If the trace has been stopped, this method returns without adding the
   * attribute. The maximum number of attributes that can be added to a Trace are {@value
   * #MAX_TRACE_CUSTOM_ATTRIBUTES}.
   *
   * @param attribute Name of the attribute
   * @param value Value of the attribute Note: This fake implementation doesn't validate the
   *     attributes to the extent to which the real implementation does.
   */
  @Override
  @Keep
  public void putAttribute(String attribute, String value) {
    FakeReporter.addReport("Trace.putAttribute", attribute, value);
    boolean noError = true;
    try {
      attribute = attribute.trim();
      value = value.trim();
    } catch (Exception e) {
      Log.e(LOG_TAG,
          String.format(
              "Can not set attribute %s with value %s (%s)", attribute, value, e.getMessage()));
      noError = false;
    }
    if (noError) {
      attributes.put(attribute, value);
    }
  }

  /**
   * Removes an already added attribute from the Traces. If the trace has been stopped, this method
   * returns without removing the attribute.
   *
   * @param attribute Name of the attribute to be removed from the running Traces.
   */
  @Override
  @Keep
  public void removeAttribute(String attribute) {
    FakeReporter.addReport("Trace.removeAttribute", attribute);
    if (isStopped()) {
      Log.e(LOG_TAG, "Can't remove a attribute from a Trace that's stopped.");
      return;
    }
    attributes.remove(attribute);
  }

  /**
   * Returns the value of an attribute.
   *
   * @param attribute name of the attribute to fetch the value for
   * @return the value of the attribute if it exists or null otherwise.
   */
  @Override
  @Nullable
  @Keep
  public String getAttribute(String attribute) {
    FakeReporter.addReport("Trace.getAttribute", attribute);
    return attributes.get(attribute);
  }

  /**
   * Returns the map of all the attributes added to this Trace.
   *
   * @return map of attributes and its values currently added to this Trace
   */
  @Override
  public Map<String, String> getAttributes() {
    return new HashMap<>(attributes);
  }
}
