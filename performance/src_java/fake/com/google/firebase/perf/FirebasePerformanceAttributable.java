package com.google.firebase.perf;

import androidx.annotation.Nullable;
import com.google.firebase.perf.metrics.Trace;
import java.util.Map;

/** Attribute functions needed for Traces and HttpMetrics */
public interface FirebasePerformanceAttributable {
  /** Maximum allowed number of attributes allowed in a trace. */
  int MAX_TRACE_CUSTOM_ATTRIBUTES = 5;
  /** Maximum allowed length of the Key of the {@link Trace} attribute */
  int MAX_ATTRIBUTE_KEY_LENGTH = 40;
  /** Maximum allowed length of the Value of the {@link Trace} attribute */
  int MAX_ATTRIBUTE_VALUE_LENGTH = 100;
  /** Maximum allowed length of the name of the {@link Trace} */
  int MAX_TRACE_NAME_LENGTH = 100;

  /**
   * Sets a String value for the specified attribute in the object's list of attributes. The maximum
   * number of attributes that can be added are {@value #MAX_TRACE_CUSTOM_ATTRIBUTES}.
   *
   * @param attribute name of the attribute. Leading and trailing white spaces if any, will be
   *     removed from the name. The name must start with letter, must only contain alphanumeric
   *     characters and underscore and must not start with "firebase_", "google_" and "ga_. The max
   *     length is limited to {@value #MAX_ATTRIBUTE_KEY_LENGTH}
   * @param value value of the attribute. The max length is limited to {@value
   *     #MAX_ATTRIBUTE_VALUE_LENGTH}
   */
  void putAttribute(String attribute, String value);

  /**
   * Returns the value of an attribute.
   *
   * @param attribute name of the attribute to fetch the value for
   * @return The value of the attribute if it exists or null otherwise.
   */
  @Nullable String getAttribute(String attribute);

  /**
   * Removes the attribute from the list of attributes.
   *
   * @param attribute name of the attribute to be removed from the global pool.
   */
  void removeAttribute(String attribute);

  /**
   * Returns the map of all the attributes currently added
   *
   * @return map of attributes and its values currently added
   */
  Map<String, String> getAttributes();
}
