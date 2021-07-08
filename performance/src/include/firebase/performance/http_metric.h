// Copyright 2021 Google LLC

#ifndef FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_HTTP_METRIC_H_
#define FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_HTTP_METRIC_H_

#include <cstdint>
#include <string>

namespace firebase {

namespace performance {

namespace internal {
class HttpMetricInternal;
}

/// @brief Identifies different HTTP methods like GET, PUT and POST.
/// For more information about these, see
/// https://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html
enum HttpMethod {
  /// Use this when the request is using the GET HTTP Method.
  kHttpMethodGet = 0,
  /// Use this when the request is using the PUT HTTP Method.
  kHttpMethodPut,
  /// Use this when the request is using the POST HTTP Method.
  kHttpMethodPost,
  /// Use this when the request is using the DELETE HTTP Method.
  kHttpMethodDelete,
  /// Use this when the request is using the HEAD HTTP Method.
  kHttpMethodHead,
  /// Use this when the request is using the PATCH HTTP Method.
  kHttpMethodPatch,
  /// Use this when the request is using the OPTIONS HTTP Method.
  kHttpMethodOptions,
  /// Use this when the request is using the TRACE HTTP Method.
  kHttpMethodTrace,
  /// Use this when the request is using the CONNECT HTTP Method.
  kHttpMethodConnect
};

/// @brief Create instances of this class to manually instrument http network
/// activity. You can also add custom attributes to the http metric
/// which help you segment your data based off of the attributes (e.g. level
/// or country).
///
/// @note This API is not meant to be interacted with at high frequency because
/// almost all API calls involve interacting with Objective-C (on iOS) or with
/// JNI (on Android) as well as allocating a new ObjC or Java object with each
/// start/stop call on this API.
class HttpMetric {
 public:
  /// Construct an HttpMetric object.
  ///
  /// @note In order to start tracing a network request, call the Start(const
  /// char* url, HttpMethod http_method) method on the object that's returned.
  HttpMetric();

  /// Construct an HttpMetric with the given url and http method and start
  /// tracing the network request.
  ///
  /// @param url The string representation of the url for the network request
  /// that's being instrumented with this http metric.
  /// @param http_method The http method of the network request that's being
  /// instrumented by this http metirc.
  HttpMetric(const char* url, HttpMethod http_method);

  /// @brief Destroys any resources allocated for a HttpMetric object.
  ~HttpMetric();

  /// @brief Move constructor.
  HttpMetric(HttpMetric&& other);

  /// @brief Move assignment operator.
  HttpMetric& operator=(HttpMetric&& other);

  /// @brief Gets whether the HttpMetric associated with this object is started.
  /// @return true if the HttpMetric is started, false if it's stopped or
  /// canceled.
  bool is_started();

  /// @brief Cancels the http metric, and makes sure it isn't logged to the
  /// backend.
  void Cancel();

  /// @brief Stops the network trace if it hasn't already been stopped, and logs
  /// it to the backend.
  void Stop();

  /// @brief Starts a network trace with the given url and HTTP method. If a
  /// network trace had previously been started using this object, the
  /// previous one is stopped before starting this one.
  ///
  /// @note All attributes, metrics, and other metadata are cleared from the
  /// metric.
  void Start(const char* url, HttpMethod http_method);

#if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)
  // We need to decouple creating and starting an HttpMetric for the Unity
  // implementation and so we expose these methods.

  /// @brief Creates a network trace with the given url and HTTP Method. If this
  /// method is called when an HttpMetric is already active (which it shouldn't)
  /// then the previous HttpMetric is cancelled.
  void Create(const char* url, HttpMethod http_method);

  /// @brief Starts the HttpMetric that was created. Does nothing if there isn't
  /// one created.
  void StartCreatedHttpMetric();
#endif  // defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)

  /// @brief Sets a custom attribute for the given network trace with the
  /// given name and value.
  ///
  /// Setting the value to nullptr will delete a previously set attribute.
  ///
  /// @param[in] attribute_name The name of the attribute you want to set.
  /// @param[in] attribute_value The value you want to set the attribute to.
  ///
  /// @note Call this method only after you've started a network trace,
  /// otherwise the attributes will not be logged.
  void SetAttribute(const char* attribute_name, const char* attribute_value);

  /// @brief Gets the value of the custom attribute identified by the given
  /// name or nullptr if it hasn't been set.
  ///
  /// @param[in] attribute_name The name of the attribute you want to retrieve
  /// the value for.
  /// @return The value of the attribute if you've set it, if not an empty
  /// string.
  std::string GetAttribute(const char* attribute_name) const;

  /// @brief Sets the HTTP Response Code (for eg. 404 or 200) of the network
  /// trace.
  ///
  /// @param[in] http_response_code The http response code of the network
  /// response.
  void set_http_response_code(int http_response_code);

  /// @brief Sets the Request Payload size in bytes for the network trace.
  ///
  /// @param[in] bytes The size of the request payload in bytes.
  ///
  /// @note Call this method only after you've started a network trace,
  /// otherwise the request payload size will not be logged.
  void set_request_payload_size(int64_t bytes);

  /// @brief Sets the Response Content Type of the network trace.
  ///
  /// @param[in] content_type The content type of the response of the http
  /// request.
  void set_response_content_type(const char* content_type);

  /// @brief Sets the Response Payload Size in bytes for the network trace.
  ///
  /// @param[in] bytes The size of the response payload in bytes.
  void set_response_payload_size(int64_t bytes);

 private:
  /// @cond FIREBASE_APP_INTERNAL
  internal::HttpMetricInternal* internal_;

  HttpMetric(const HttpMetric& src);
  HttpMetric& operator=(const HttpMetric& src);
  /// @endcond
};

}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_HTTP_METRIC_H_
