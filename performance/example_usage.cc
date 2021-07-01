// Copyright 2021 Google LLC

#include <iostream>

#include "app/src/include/firebase/app.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/include/firebase/performance/http_metric.h"
#include "performance/src/include/firebase/performance/trace.h"

int main() {
  // Initialize
  firebase::InitResult fireperf_init_result =
      firebase::performance::Initialize(*firebase::App::GetInstance());

  if (fireperf_init_result ==
      firebase::InitResult::kInitResultFailedMissingDependency) {
    std::cout << "Failed to initialize firebase performance." << std::endl;
  } else {
    std::cout << "Successfully initialized firebase performance." << std::endl;
  }

  // Enable firebase performance monitoring.
  firebase::performance::SetPerformanceCollectionEnabled(true);

  // Disable firebase performance monitoring.
  firebase::performance::SetPerformanceCollectionEnabled(false);

  if (firebase::performance::GetPerformanceCollectionEnabled()) {
    std::cout << "Firebase Performance monitoring is enabled." << std::endl;
  } else {
    std::cout << "Firebase Performance monitoring is disabled." << std::endl;
  }

  // Create and start a Trace on the heap, add custom attributes, metrics.
  auto trace =
      new firebase::performance::Trace("myMethod");  // Also starts the trace.
  std::cout << "Trace started status: " << trace->is_started() << std::endl;
  trace->IncrementMetric("cacheHit", 2);
  trace->SetMetric("cacheSize", 50);
  // Currently returns a stub value, but it should return 50 when a fake is
  // implemented.
  std::cout << "Value of  the \"cacheSize\" metric: "
            << trace->GetLongMetric("cacheSize") << std::endl;

  trace->SetAttribute("level", "4");
  trace->GetAttribute("level");  // returns "4"
  trace->SetAttribute("level", nullptr);

  // Stop trace, and re-use the object for another trace.
  trace->Start("myOtherMethod");

  delete trace;  // Logs myOtherMethod and deletes the object.

  // Create a Trace on the heap, start it later and then stop it.
  auto delayed_start_trace = new firebase::performance::Trace();
  // Do some set up work that we don't want included in the trace duration.

  // Once we're ready, start.
  delayed_start_trace->Start("criticalSectionOfCode");

  // Interesting code ends.
  delete delayed_start_trace;  // Stops and logs it to the backend.

  // Trace using automatic storage (in this case on the stack).
  {
    firebase::performance::Trace trace_stack("myMethod");
    trace_stack.IncrementMetric("cacheHit", 2);
    trace_stack.SetMetric("cacheSize", 50);
    // Currently returns a stub value, but it should return 50 when a fake is
    // implemented.
    std::cout << "Value of the \"cacheSize\" metric: "
              << trace_stack.GetLongMetric("cacheSize") << std::endl;

    trace_stack.SetAttribute("level", "4");
    // Currently returns a stub value, but should return 4 when a fake is
    // implemented.
    std::cout << "Value of \"level\" attribute on the \"myMethod\" trace: "
              << trace_stack.GetAttribute("level") << std::endl;
    trace_stack.SetAttribute("level", nullptr);
    std::cout << "Trace started status: " << trace_stack.is_started()
              << std::endl;
  }
  // Stop is called when it's destructed, and the trace is logged to the
  // backend.

  // Trace on the stack, and start it later.
  {
    firebase::performance::Trace trace_stack;

    trace_stack.Start("someTrace");
    trace_stack.IncrementMetric("cacheHit", 2);

    trace_stack.Start(
        "someOtherTrace");  // Logs someTrace, and starts "someOtherTrace"
    trace_stack.Cancel();   // Cancel someOtherTrace.
    std::cout << "Trace started status: " << trace_stack.is_started()
              << std::endl;
  }

  // Create an HttpMetric, custom attributes, counters and add details.
  // Note: Only needed if developer is using non-standard networking library

  // On the heap
  auto http_metric = new firebase::performance::HttpMetric(
      "https://google.com", firebase::performance::HttpMethod::kHttpMethodGet);

  // Add more detail to http metric
  http_metric->set_http_response_code(200);
  http_metric->set_request_payload_size(25);
  http_metric->set_response_content_type("application/json");
  http_metric->set_response_payload_size(500);

  std::cout << "HttpMetric started status: " << http_metric->is_started()
            << std::endl;

  http_metric->SetAttribute("level", "4");
  // Currently returns a stub value, but should return 4 when a fake is
  // implemented.
  std::cout
      << "Value of \"level\" attribute on the \"google.com\" http metric: "
      << http_metric->GetAttribute("level") << std::endl;

  // Logs the google.com http metric and starts a new one for a different
  // network request.
  http_metric->Start("https://firebase.com",
                     firebase::performance::HttpMethod::kHttpMethodPost);
  http_metric->set_response_payload_size(500);

  delete http_metric;  // Stops and logs it to the backend.

  // create an http metric object on the heap, but start it later.
  auto http_metric_delayed_start = new firebase::performance::HttpMetric();

  // Do some setup.

  // Start the trace.
  http_metric_delayed_start->Start(
      "https://firebase.com",
      firebase::performance::HttpMethod::kHttpMethodGet);

  // Stop it.
  http_metric_delayed_start->Stop();

  // HttpMetric using Automatic storage (in this case on the stack), restarted
  // so that the first one is logged, and then the new one is cancelled which is
  // not logged.
  {
    // This also starts the HttpMetric.
    firebase::performance::HttpMetric http_metric_stack(
        "https://google.com",
        firebase::performance::HttpMethod::kHttpMethodGet);

    // Add more detail to http metric
    http_metric_stack.set_http_response_code(200);
    http_metric_stack.set_request_payload_size(25);
    http_metric_stack.set_response_content_type("application/json");
    http_metric_stack.set_response_payload_size(500);

    http_metric_stack.SetAttribute("level", "4");
    // Currently returns a stub value, but should return 4 when a fake is
    // implemented.
    std::cout
        << "Value of \"level\" attribute on the \"google.com\" http metric: "
        << http_metric_stack.GetAttribute("level") << std::endl;

    // Stops the google.com http metric and starts a new one that tracks the
    // firebase.com network request.
    http_metric_stack.Start("https://firebase.com",
                            firebase::performance::HttpMethod::kHttpMethodPost);

    std::cout << "HttpMetric started status: " << http_metric_stack.is_started()
              << std::endl;

    // Cancels the new firebase.com network trace, because it doesn't have any
    // valid data.
    http_metric_stack.Cancel();

    std::cout << "HttpMetric started status: " << http_metric_stack.is_started()
              << std::endl;
  }

  // HttpMetric on stack is stopped and logged when it's destroyed.
  {
    firebase::performance::HttpMetric http_metric_stack;

    http_metric_stack.Start("https://google.com",
                            firebase::performance::HttpMethod::kHttpMethodGet);

    // Add more detail to http metric
    http_metric_stack.set_http_response_code(200);
  }  // HttpMetric is stopped and logged to the backend as part of being
     // destroyed.

  return 0;
}
