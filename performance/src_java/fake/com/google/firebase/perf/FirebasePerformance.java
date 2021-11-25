package com.google.firebase.perf;

import androidx.annotation.StringDef;
import com.google.firebase.perf.metrics.HttpMetric;
import com.google.firebase.perf.metrics.Trace;
import com.google.firebase.testing.cppsdk.FakeReporter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The Firebase Performance API. This is a fake implementation. */
public class FirebasePerformance {
  private static volatile FirebasePerformance firebasePerformance;
  private static boolean performanceCollectionEnabled = true;

  /** Valid HttpMethods for manual network APIs */
  @StringDef({HttpMethod.GET, HttpMethod.PUT, HttpMethod.POST, HttpMethod.DELETE, HttpMethod.HEAD,
      HttpMethod.PATCH, HttpMethod.OPTIONS, HttpMethod.TRACE, HttpMethod.CONNECT})
  @Retention(RetentionPolicy.SOURCE)
  public @interface HttpMethod {
    String GET = "GET";
    String PUT = "PUT";
    String POST = "POST";
    String DELETE = "DELETE";
    String HEAD = "HEAD";
    String PATCH = "PATCH";
    String OPTIONS = "OPTIONS";
    String TRACE = "TRACE";
    String CONNECT = "CONNECT";
  }

  /**
   * Returns a singleton of FirebasePerformance.
   *
   * @return the singleton FirebasePerformance object.
   */
  public static FirebasePerformance getInstance() {
    FakeReporter.addReport("FirebasePerformance.getInstance");
    if (firebasePerformance == null) {
      synchronized (FirebasePerformance.class) {
        if (firebasePerformance == null) {
          firebasePerformance = new FirebasePerformance();
        }
      }
    }
    return firebasePerformance;
  }

  /**
   * Creates a Trace object with given name and start the trace.
   *
   * @param traceName name of the trace. Requires no leading or trailing whitespace, no leading
   *     underscore [_] character, max length of {@value #MAX_TRACE_NAME_LENGTH} characters.
   * @return the new Trace object.
   */
  public static Trace startTrace(String traceName) {
    Trace trace = Trace.create(traceName);
    trace.start();
    return trace;
  }

  /**
   * Enables or disables performance monitoring. In the fake version this is just a boolean that's
   * in memory that toggles this state.
   *
   * @param enable Should performance monitoring be enabled
   */
  public void setPerformanceCollectionEnabled(boolean enable) {
    FakeReporter.addReport(
        "FirebasePerformance.setPerformanceCollectionEnabled", Boolean.toString(enable));
    performanceCollectionEnabled = enable;
  }

  /**
   * Determines whether performance monitoring is enabled or disabled. This respects the Firebase
   * Performance specific values first, and if these aren't set, uses the Firebase wide data
   * collection switch.
   *
   * @return true if performance monitoring is enabled and false if performance monitoring is
   *     disabled. This is for dynamic enable/disable state. This does not reflect whether
   *     instrumentation is enabled/disabled in Gradle properties.
   */
  public boolean isPerformanceCollectionEnabled() {
    FakeReporter.addReport("FirebasePerformance.isPerformanceCollectionEnabled");
    return performanceCollectionEnabled;
  }

  /**
   * Creates a Trace object with given name.
   *
   * @param traceName name of the trace, requires no leading or trailing whitespace, no leading
   *     underscore '_' character, max length is {@value #MAX_TRACE_NAME_LENGTH} characters.
   * @return the new Trace object.
   */
  public Trace newTrace(String traceName) {
    return Trace.create(traceName);
  }

  /**
   * Creates a HttpMetric object for collecting network performance data for one request/response
   *
   * @param url a valid url String, cannot be empty
   * @param httpMethod One of the values GET, PUT, POST, DELETE, HEAD, PATCH, OPTIONS, TRACE, or
   *     CONNECT
   * @return the new HttpMetric object.
   */
  public HttpMetric newHttpMetric(String url, @HttpMethod String httpMethod) {
    return new HttpMetric(url, httpMethod);
  }
}
