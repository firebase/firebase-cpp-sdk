package com.google.firebase.perf.metrics;

import android.util.Log;
import androidx.annotation.Nullable;
import com.google.apps.tiktok.testing.errorprone.SuppressViolation;
import com.google.firebase.perf.FirebasePerformance.HttpMethod;
import com.google.firebase.perf.FirebasePerformanceAttributable;
import com.google.firebase.testing.cppsdk.FakeReporter;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Metric used to collect data for network requests/responses. A new object must be used for every
 * request/response. This class is not thread safe.
 */
public class HttpMetric implements FirebasePerformanceAttributable {
  private static final String LOG_TAG = "FirebasePerformanceFake";

  private final Map<String, String> attributes;
  private boolean isStopped = false;

  /** Constructs a new NetworkRequestMetricBuilder object given a String url value */
  public HttpMetric(String url, @HttpMethod String httpMethod) {
    FakeReporter.addReport("new HttpMetric", url, httpMethod);
    attributes = new ConcurrentHashMap<>();
  }

  /**
   * Sets the httpResponse code of the request
   *
   * @param unusedResponseCode valid values are greater than 0. Invalid usage will be logged.
   */
  public void setHttpResponseCode(int unusedResponseCode) {
    FakeReporter.addReport("HttpMetric.setHttpResponseCode", Integer.toString(unusedResponseCode));
  }

  /**
   * Sets the size of the request payload
   *
   * @param bytes valid values are greater than or equal to 0. Invalid usage will be logged.
   */
  public void setRequestPayloadSize(long bytes) {
    FakeReporter.addReport("HttpMetric.setRequestPayloadSize", Long.toString(bytes));
  }

  /**
   * Sets the size of the response payload
   *
   * @param bytes valid values are greater than or equal to 0. Invalid usage will be logged.
   */
  public void setResponsePayloadSize(long bytes) {
    FakeReporter.addReport("HttpMetric.setResponsePayloadSize", Long.toString(bytes));
  }

  /**
   * Content type of the response such as text/html, application/json, etc...
   *
   * @param contentType valid string of MIME type. Invalid usage will be logged.
   */
  public void setResponseContentType(@Nullable String contentType) {
    FakeReporter.addReport("HttpMetric.setResponseContentType", contentType);
  }

  /** Marks the start time of the request */
  public void start() {
    FakeReporter.addReport("HttpMetric.start");
  }

  /**
   * Marks the end time of the response and queues the network request metric on the device for
   * transmission. Check logcat for transmission info.
   */
  public void stop() {
    FakeReporter.addReport("HttpMetric.stop");
    if (!isStopped) {
      isStopped = true;
    } else {
      Log.w(LOG_TAG, "Tried to stop http metric after it had already been stopped.");
    }
  }

  /**
   * Sets a String value for the specified attribute. Updates the value of the attribute if the
   * attribute already exists. If the HttpMetric has been stopped, this method returns without
   * adding the attribute. The maximum number of attributes that can be added to a HttpMetric are
   * {@value #MAX_TRACE_CUSTOM_ATTRIBUTES}.
   *
   * @param attribute name of the attribute
   * @param value value of the attribute
   */
  @SuppressViolation("catch_specific_exceptions")
  @Override
  public void putAttribute(String attribute, String value) {
    FakeReporter.addReport("HttpMetric.putAttribute", attribute, value);
    attribute = attribute.trim();
    value = value.trim();
    attributes.put(attribute, value);
  }

  /**
   * Removes an already added attribute from the HttpMetric. If the HttpMetric has already been
   * stopped, this method returns without removing the attribute.
   *
   * @param attribute name of the attribute to be removed from the running Traces.
   */
  @Override
  public void removeAttribute(String attribute) {
    FakeReporter.addReport("HttpMetric.removeAttribute", attribute);
    if (isStopped) {
      Log.e(LOG_TAG, "Can't remove a attribute from a HttpMetric that's stopped.");
      return;
    }
    attributes.remove(attribute);
  }

  /**
   * Returns the value of an attribute.
   *
   * @param attribute name of the attribute to fetch the value for
   * @return The value of the attribute if it exists or null otherwise.
   */
  @Override
  @Nullable
  public String getAttribute(String attribute) {
    FakeReporter.addReport("HttpMetric.getAttribute", attribute);
    return attributes.get(attribute);
  }

  /**
   * Returns the map of all the attributes added to this HttpMetric.
   *
   * @return map of attributes and its values currently added to this HttpMetric
   */
  @Override
  public Map<String, String> getAttributes() {
    return new HashMap<>(attributes);
  }
}
