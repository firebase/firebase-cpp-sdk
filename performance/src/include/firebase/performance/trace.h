// Copyright 2021 Google LLC

#ifndef FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_TRACE_H_
#define FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_TRACE_H_

#include <cstdint>
#include <string>

#include "firebase/performance.h"

namespace firebase {

namespace performance {

namespace internal {
class TraceInternal;
}

/// @brief Create instances of a trace to manually instrument any arbitrary
/// section of your code. You can also add custom attributes to the trace
/// which help you segment your data based off of the attributes (for eg. level
/// or country) and you also have the ability to add custom metrics (for eg.
/// cache hit count).
///
/// @note This API is not meant to be interacted with at high frequency because
/// almost all API calls involve interacting with Objective-C (on iOS) or with
/// JNI (on Android).
class Trace {
 public:
  /// Constructs a trace object. In order to start a trace, please call the
  /// Start("traceName") method on the object that this constructor returns.
  explicit Trace();

  /// Constructs a trace with the given name and starts it.
  ///
  /// @param name Name of the trace. The name cannot begin with _ and has to be
  /// less than 100 characters in length.
  explicit Trace(const char* name);

  /// @brief Destroys any resources allocated for a Trace object.
  ~Trace();

  /// @brief Move constructor.
  Trace(Trace&& other);

  /// @brief Move assignment operator.
  Trace& operator=(Trace&& other);

  /// @brief Gets whether the Trace associated with this object is started.
  /// @return true if the Trace is started, false if it's stopped or
  /// canceled.
  bool is_started();

  /// @brief Cancels the trace, so that it isn't logged to the backend.
  void Cancel();

  /// @brief Stops the trace and logs it to the backend. If you haven't
  /// cancelled a trace, this happens automatically when the object is
  /// destroyed.
  void Stop();

  /// @brief Starts a trace with the given name. If a trace had previously been
  /// started using the same object, that previous trace is stopped before
  /// starting this one.
  ///
  /// @note None of the associated attributes, metrics, and other metadata is
  /// carried over to a new Trace if one is created.
  void Start(const char* name);

#if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)
  // We need to decouple creating and starting an HttpMetric for the Unity
  // implementation and so we expose these methods.

  /// @brief Creates a trace with the given name. If this method is called when
  /// a Trace is already active (which it shouldn't) then the previous Trace is
  /// cancelled.
  void Create(const char* name);

  /// @brief Starts the Trace that was created. Does nothing if there isn't
  /// one created.
  void StartCreatedTrace();
#endif  // defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)

  /// @brief Gets the value of the metric identified by the metric_name or 0
  /// if it hasn't yet been set.
  ///
  /// @param[in] metric_name The name of the metric to get the value of.
  /// @return The previously set of the given metric or 0 if it hasn't been
  /// set.
  int64_t GetLongMetric(const char* metric_name) const;

  /// @brief Increments the existing value of the given metric by
  /// increment_by or sets it to increment_by if the metric hasn't been set.
  ///
  /// @param[in] metric_name The name of the metric to increment the value
  /// of.
  /// @param[in] increment_by The value by which the metric should be
  /// incremented.
  void IncrementMetric(const char* metric_name, const int64_t increment_by);

  /// @brief Sets the value of the given metric to metric_value.
  ///
  /// @param[in] metric_name The name of the metric to set the value of.
  /// @param[in] metric_value The value to set the metric to.
  void SetMetric(const char* metric_name, const int64_t metric_value);

  /// @brief Creates a custom attribute for the given trace with the given name
  /// and value.
  ///
  /// Setting the value to nullptr will delete a previously set attribute.
  /// @param[in] attribute_name The name of the attribute you want to set.
  /// @param[in] attribute_value The value you want to set the attribute to.
  void SetAttribute(const char* attribute_name, const char* attribute_value);

  /// @brief Gets the value of the custom attribute identified by the given
  /// name or nullptr if it hasn't been set.
  ///
  /// @param[in] attribute_name The name of the attribute you want to retrieve
  /// the value for.
  /// @return The value of the attribute if you've set it, if not an empty
  /// string.
  std::string GetAttribute(const char* attribute_name) const;

 private:
  /// @cond FIREBASE_APP_INTERNAL
  internal::TraceInternal* internal_;

  Trace(const Trace& src);
  Trace& operator=(const Trace& src);
  /// @endcond
};

}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_TRACE_H_
