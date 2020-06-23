/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app/rest/transport_curl.h"

#include <cassert>
#include <deque>
#include <map>

#include "app/rest/controller_curl.h"
#include "app/rest/util.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/mutex.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"
#include "app/src/util.h"
#include "curl/curl.h"

#if FIREBASE_PLATFORM_WINDOWS
#include "Winsock2.h"
#else
#include <sys/select.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

namespace firebase {
namespace rest {

// An action requested to be performed in the background thread.
enum RequestedAction {
  // Perform the transfer.
  kRequestedActionPerform,
  // Cancel an in-progress transfer.
  kRequestedActionCancel,
  // Pause an in-progress transfer.
  kRequestedActionPause,
  // Pause an in-progress transfer.
  kRequestedActionResume,
  // Quit the background thread.
  kRequestedActionQuit,
};

// A structure representing a pending TranportCurl::Perform call.
struct TransportCurlActionData {
  explicit TransportCurlActionData()
      : transport(nullptr),
        action(kRequestedActionPerform),
        curl(nullptr),
        request(nullptr),
        response(nullptr),
        controller(nullptr) {}
  // Transport that scheduled this request.
  // Required by:
  // * kRequestedActionPerform
  // * kRequestedActionCancel
  // * kRequestedActionPause
  // * kRequestedActionResume
  TransportCurl* transport;
  // Action to perform on the curl transfer thread.
  RequestedAction action;
  // Pointer to the curl object to operate on.
  // Required by:
  // * kRequestedActionPerform
  // * kRequestedActionCancel
  // * kRequestedActionPause
  // * kRequestedActionResume
  CURL* curl;
  // Data to send to the server as the request.
  // Required by kRequestedActionPerform.
  Request* request;
  // Data received from the server as the response.
  // Required by:
  // * kRequestedActionPerform
  // * kRequestedActionCancel
  // * kRequestedActionPause
  // * kRequestedActionResume
  Response* response;
  // Pointer to the controller.
  // Optionally used by kRequestedActionPerform.
  ControllerCurl* controller;

  // Create a quit action.
  static TransportCurlActionData Quit() {
    TransportCurlActionData transport_action;
    transport_action.action = kRequestedActionQuit;
    return transport_action;
  }

  // Create a perform action.
  static TransportCurlActionData Perform(TransportCurl* transport_curl,
                                         Request* request, Response* response,
                                         CURL* curl,
                                         ControllerCurl* controller = nullptr) {
    TransportCurlActionData transport_action;
    transport_action.transport = transport_curl;
    transport_action.action = kRequestedActionPerform;
    transport_action.curl = curl;
    transport_action.request = request;
    transport_action.response = response;
    transport_action.controller = controller;
    return transport_action;
  }

  // Create a cancellation action.
  static TransportCurlActionData Cancel(TransportCurl* transport_curl,
                                        Response* response, CURL* curl) {
    return ResponseAction(transport_curl, kRequestedActionCancel, response,
                          curl);
  }

  // Create a pause action.
  static TransportCurlActionData Pause(TransportCurl* transport_curl,
                                       Response* response, CURL* curl) {
    return ResponseAction(transport_curl, kRequestedActionPause, response,
                          curl);
  }

  // Create a resume action.
  static TransportCurlActionData Resume(TransportCurl* transport_curl,
                                        Response* response, CURL* curl) {
    return ResponseAction(transport_curl, kRequestedActionResume, response,
                          curl);
  }

 private:
  // Create an action associated with a response.
  static TransportCurlActionData ResponseAction(TransportCurl* transport_curl,
                                                RequestedAction action,
                                                Response* response,
                                                CURL* curl) {
    TransportCurlActionData transport_action;
    transport_action.transport = transport_curl;
    transport_action.action = action;
    transport_action.curl = curl;
    transport_action.response = response;
    return transport_action;
  }
};

// The data needed to run a curl request in the background. When this class
// receives a curl handle it takes ownership and is responsible for running the
// request and deallocating the resources when the request is complete.
class BackgroundTransportCurl {
  // Callback that completes a response.
  typedef void (*CompleteFunction)(BackgroundTransportCurl* transport,
                                   void* data);

 public:
  BackgroundTransportCurl(CURLM* curl_multi, CURL* curl, Request* request,
                          Response* response, Mutex* controller_mutex,
                          ControllerCurl* controller,
                          TransportCurl* transport_curl,
                          CompleteFunction complete, void* complete_data);
  ~BackgroundTransportCurl();
  bool PerformBackground(Request* request);

  CURL* curl() const { return curl_; }
  Response* response() const { return response_; }
  void set_canceled(bool canceled) { canceled_ = canceled; }
  void set_timed_out(bool timed_out) { timed_out_ = timed_out; }
  ControllerCurl* controller() const { return controller_; }
  TransportCurl* transport_curl() const { return transport_curl_; }

 private:
  void CheckOk(CURLcode code, const char* msg);
  void CompleteOperation();

 private:
  CURLM* curl_multi_;
  // The Curl handler
  CURL* curl_;
  // Error buffer
  char err_buf_[CURL_ERROR_SIZE];
  // Curl error code
  CURLcode err_code_;
  // A linked list to set custom HTTP headers in the request.
  curl_slist* request_header_;
  // Request being performed.
  Request* request_;
  // The response that needs to be completed.
  Response* response_;
  // Guards controller_.
  Mutex* controller_mutex_;
  // Controller associated with the transfer.
  ControllerCurl* controller_;
  TransportCurl* transport_curl_;
  // If the transport was synchronous, block using this semaphore.
  // TODO(b/67899466): Remove this once we transition to exclusively
  // asynchronous curl requests.
  Semaphore* semaphore_;
  CompleteFunction complete_;
  void* complete_data_;
  // Whether the operation has been canceled.
  bool canceled_;
  // Whether the operation timed out.
  bool timed_out_;
};

// The data common to both threads. This is used to communicate when the
// background thread should shut down, and when new requests have come in.
class CurlThread {
 public:
  CurlThread();
  // Shut down the request processing thread.
  ~CurlThread();

  // Schedule an action on the ProcessRequests thread.
  void ScheduleAction(const TransportCurlActionData& action_data);

  // Cancel a request or flush scheduled matching requests.
  int CancelRequest(TransportCurl* transport_curl, Response* response,
                    CURL* curl);

 private:
  // Pull the next request from the queue, optionally blocking if the queue
  // semaphore has no remaining grants.  Returns true if an action was returned,
  // false otherwise.
  bool GetNextAction(TransportCurlActionData* data,
                     int64_t wait_for_milliseconds);

  // Add a request to the set of running requests.
  void AddTransfer(BackgroundTransportCurl* transport);
  // Remove a response from the set of running responses.
  // Returns a transport if the response is found in the set of running
  // requests.
  BackgroundTransportCurl* RemoveTransfer(Response* response);

  // Cancel all outstanding requests.
  void CancelAllTransfers();

  Mutex* mutex() { return &mutex_; }

  // Process requests from action_data_ the see the function definition for the
  // complete documentation.
  void ProcessRequests();

  // Thread entry point that calls ProcessRequests.
  static void ProcessRequests(void* thread);

 private:
  flatbuffers::unique_ptr<Thread> background_thread_;
  // Guards mutation of action_data_queue_, responses_ and
  // controller_ pointers in BackgroundTransportCurl instances.
  Mutex mutex_;
  // When signalled the thread will pull the next item from action_data.
  // Should be signalled for each item added to the action_data queue.
  // This is also signalled
  Semaphore action_data_signal_;
  std::deque<TransportCurlActionData> action_data_queue_;
  // Transports for in progress requests for each response.  This allows all
  // requests to be canceled when this object is cleaned up.
  std::map<Response*, BackgroundTransportCurl*> transport_by_response_;
  // Polling interval while requests are in progress.
  static const int64_t kPollIntervalMilliseconds;
};

namespace {

// Called when curl has received the header from the server.
size_t CurlHeaderCallback(char* buffer, size_t size, size_t nitems,
                          void* userdata) {
  FIREBASE_ASSERT_RETURN(0, userdata != nullptr);
  Response* response = static_cast<Response*>(userdata);
  // Size is always 1, see https://curl.haxx.se/mail/lib-2010-12/0123.html.
  if (response->ProcessHeader(buffer, size * nitems)) {
    return size * nitems;
  } else {
    return 0;
  }
}

// Called when curl has received more data from the server.
size_t CurlWriteCallback(char* buffer, size_t size, size_t nmemb,
                         void* userdata) {
  FIREBASE_ASSERT_RETURN(0, userdata != nullptr);
  Response* response = static_cast<Response*>(userdata);
  // Size is always 1, see https://curl.haxx.se/mail/lib-2010-12/0123.html.
  if (response->ProcessBody(buffer, size * nmemb)) {
    return size * nmemb;
  } else {
    return 0;
  }
}

// Called when curl is ready to send more data to the server.
size_t CurlReadCallback(char* buffer, size_t size, size_t nitems,
                        void* userdata) {
  FIREBASE_ASSERT_RETURN(0, userdata != nullptr);
  Request* request = reinterpret_cast<Request*>(userdata);
  bool abort;
  size_t data_read = request->ReadBody(buffer, size * nitems, &abort);
  return abort ? CURL_READFUNC_ABORT : data_read;
}

// Data accessible by both threads.
CurlThread* g_curl_thread = nullptr;

// Count initializations that multiple libraries can use this simultaneously.
int g_initialize_count = 0;

// Mutex for Curl initialization.
Mutex g_initialize_mutex;  // NOLINT

}  // namespace

void InitTransportCurl() {
  MutexLock lock(g_initialize_mutex);
  if (g_initialize_count == 0) {
    // Initialize curl.
    CURLcode global_init_code = curl_global_init(CURL_GLOBAL_ALL);
    FIREBASE_ASSERT_MESSAGE(global_init_code == CURLE_OK,
                            "curl global init failed with code %d",
                            global_init_code);

    // Kick off background thread.
    assert(!g_curl_thread);
    g_curl_thread = new CurlThread();
  }
  g_initialize_count++;
}

void CleanupTransportCurl() {
  MutexLock lock(g_initialize_mutex);
  assert(g_initialize_count > 0);
  g_initialize_count--;
  if (g_initialize_count == 0) {
    // Shut down background thread.
    delete g_curl_thread;
    g_curl_thread = nullptr;

    // Clean up curl.
    curl_global_cleanup();
  }
}

BackgroundTransportCurl::BackgroundTransportCurl(
    CURLM* curl_multi, CURL* curl, Request* request, Response* response,
    Mutex* controller_mutex, ControllerCurl* controller,
    TransportCurl* transport_curl, CompleteFunction complete,
    void* complete_data)
    : curl_multi_(curl_multi),
      curl_(curl),
      err_code_(CURLE_OK),
      request_header_(nullptr),
      request_(request),
      response_(response),
      controller_mutex_(controller_mutex),
      controller_(controller),
      transport_curl_(transport_curl),
      complete_(complete),
      complete_data_(complete_data),
      canceled_(false),
      timed_out_(false) {
  assert(curl_multi_);
  assert(curl_);
  assert(transport_curl);
  FIREBASE_ASSERT_MESSAGE(curl_ != nullptr,
                          "failed to start a curl easy session");
  // Set the error buffer before all other curl_easy_setopt since CheckOk()
  // depends on it.
  err_buf_[0] = '\0';  // Make sure it is empty at beginning.
  if (controller_) {
    controller_->InitializeControllerHandle(&controller_, controller_mutex);
  }
}

BackgroundTransportCurl::~BackgroundTransportCurl() {
  // SignalTransfer complete can tear down the TransportCurl referenced by this
  // object, we need to remove all references to objects that could be destroyed
  // by TransportCurl.
  {
    MutexLock lock(*controller_mutex_);
    if (controller_) {
      controller_->InitializeControllerHandle(nullptr, nullptr);
      controller_->set_transferring(false);
    }
  }
  curl_multi_remove_handle(curl_multi_, curl_);
  if (request_header_) {
    curl_slist_free_all(request_header_);
    request_header_ = nullptr;
  }

  // If this is an asynchronous operation, MarkFailed() or MarkCompleted()
  // could end up attempting to tear down TransportCurl so we signal
  // completion here.
  if (transport_curl_->is_async()) {
    transport_curl_->SignalTransferComplete();
    CompleteOperation();
  } else {
    // Synchronous operations need all data present in the response before
    // Perform() returns so signal complete after MarkFailed() or
    // MarkCompleted().
    CompleteOperation();
    transport_curl_->SignalTransferComplete();
  }
}

void BackgroundTransportCurl::CompleteOperation() {
  if (complete_) complete_(this, complete_data_);
  if (canceled_) {
    response_->set_status(rest::util::HttpNoContent);
    request_->MarkFailed();
    response_->MarkFailed();
  } else if (timed_out_) {
    response_->set_status(rest::util::HttpRequestTimeout);
    request_->MarkFailed();
    response_->MarkFailed();
  } else {
    request_->MarkCompleted();
    response_->MarkCompleted();
  }
}

void BackgroundTransportCurl::CheckOk(CURLcode code, const char* msg) {
  if (code == CURLE_OK) {
    return;
  }

  LogError("failed to %s with error code (%d) %s", msg, code, err_buf_);
  if (err_code_ == CURLE_OK) {
    // We only keep the first error code. Subsequent error may be caused by
    // previous error and thus less helpful for debug.
    err_code_ = code;
  }
}

bool BackgroundTransportCurl::PerformBackground(Request* request) {
  RequestOptions& options = request->options();
  CheckOk(curl_easy_setopt(curl_, CURLOPT_ERRORBUFFER, err_buf_),
          "set error buffer");

  // Ensure that we are only accepting HTTP(S) traffic.
  // TODO(73540740): Remove support for HTTP.
  CheckOk(curl_easy_setopt(curl_, CURLOPT_PROTOCOLS,
                           CURLPROTO_HTTP | CURLPROTO_HTTPS),
          "set valid protocols");

  // Verify SSL.
  CheckOk(curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L), "verify peer");
  CheckOk(curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L), "verify host");

#ifdef FIREBASE_SSL_CAPATH
  CheckOk(curl_easy_setopt(curl_, CURLOPT_CAPATH, FIREBASE_SSL_CAPATH),
          "CA Path");
#endif

  // Set callback functions.
  CheckOk(curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, CurlHeaderCallback),
          "set http header callback");
  CheckOk(curl_easy_setopt(curl_, CURLOPT_HEADERDATA, response_),
          "set http header callback data");
  CheckOk(curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, CurlWriteCallback),
          "set http body write callback");
  CheckOk(curl_easy_setopt(curl_, CURLOPT_WRITEDATA, response_),
          "set http body write callback data");
  CheckOk(curl_easy_setopt(curl_, CURLOPT_READFUNCTION, CurlReadCallback),
          "set http body read callback");
  CheckOk(curl_easy_setopt(curl_, CURLOPT_READDATA, request),
          "set http body read callback data");
  CheckOk(curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, options.timeout_ms),
          "set http timeout milliseconds");

  // curl library is using http2 as default, so need to specify this.
  CheckOk(curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1),
          "set http version to http1");

  // SDK error in initialization stage is not recoverable.
  FIREBASE_ASSERT(err_code_ == CURLE_OK);

  err_code_ = CURLE_OK;

  // Set VERBOSE for debug; does not affect the HTTP connection.
  if (options.verbose) {
    curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);
  }

  if (request_header_) {
    curl_slist_free_all(request_header_);
    request_header_ = nullptr;
  }

  for (const auto& pair : options.header) {
    std::string header;
    header.reserve(pair.first.size() + pair.second.size() + 1);
    header.append(pair.first);
    header.append(1, util::kHttpHeaderSeparator);
    header.append(pair.second);
    request_header_ = curl_slist_append(request_header_, header.c_str());
  }
  std::string method = util::ToUpper(options.method);
  if (method == util::kPost && request_->options().stream_post_fields) {
    size_t transfer_size = request_->GetPostFieldsSize();
    // If the upload size is unknown use chunked encoding.
    if (transfer_size == ~static_cast<size_t>(0)) {
      // To use POST to a HTTP 1.1 sever we need to use chunked encoding.
      // This is not supported by HTTP 1.0 servers which need to know the size
      // of the request up front.
      request_header_ =
          curl_slist_append(request_header_, "Transfer-Encoding: chunked");
    } else {
      // Set the expected POST size, to be compatible with HTTP 1.0 servers.
      curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                       static_cast<long>(transfer_size));  // NOLINT
      if (controller_) {
        controller_->set_transfer_size(static_cast<int64_t>(transfer_size));
      }
    }
  }

  if (request_header_ != nullptr) {
    CheckOk(curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, request_header_),
            "set http header");
  }

  // Set PRIVATE pointer, so that we can get the response when the request
  // completes.
  CheckOk(curl_easy_setopt(curl_, CURLOPT_PRIVATE, this),
          "set private pointer");
  // Set URL
  CheckOk(curl_easy_setopt(curl_, CURLOPT_URL, options.url.c_str()),
          "set http url");
  // Set the method.
  if (method == util::kGet) {
    CheckOk(curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L), "set http method");
  } else if (method == util::kPost) {
    CheckOk(curl_easy_setopt(curl_, CURLOPT_POST, 1L), "set http method");
  } else {
    // For all other method. Note: If your code is depending on a documented
    // method e.g. PUT, try add another else-if branch above instead of relying
    // on this generic mechanism.
    CheckOk(curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, method.c_str()),
            "set http method");
  }

  // If streaming is disabled, copy post fields into a string to upload in a
  // single chunk.
  std::string post_fields_temp;
  if (!options.stream_post_fields &&
      request_->ReadBodyIntoString(&post_fields_temp)) {
    // If the request's post_fields are passed to ReadBodyIntoString, it takes
    // an early out because it assumes the work is already done.
    // However we still need it to do the compression, so, we have to write to a
    // temporary and then assign it to the request's post_fields.
    request_->options().post_fields = post_fields_temp;

    // Specify the size to make sure the whole post_fields is consumed.
    CheckOk(curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                             options.post_fields.size()),
            "set http post fields");
    CheckOk(curl_easy_setopt(curl_, CURLOPT_POSTFIELDS,
                             options.post_fields.c_str()),
            "set http post fields");
    if (controller_) {
      controller_->set_transfer_size(
          static_cast<int64_t>(options.post_fields.size()));
    }
  }

  if (err_code_ == CURLE_OK) {
    // Add the easy handle to the multi handle to prepare operation.
    return curl_multi_add_handle(curl_multi_, curl_) == CURLM_OK;
  } else {
    // If any error happens during the setup, we do not perform http request and
    // return immediately instead.
    response_->set_sdk_error_code(err_code_);
    set_canceled(true);
    return false;
  }
}

TransportCurl::TransportCurl()
    : is_async_(false), running_transfers_(0), running_transfers_semaphore_(0) {
  curl_ = util::CreateCurlPtr();
  assert(curl_ != nullptr);  // Failed to get curl pointer.  Something is wrong.
}

TransportCurl::~TransportCurl() {
  WaitForAllTransfersToComplete();
  util::DestroyCurlPtr(curl_);
}

// Default polling interval while requests are in progress.
const int64_t CurlThread::kPollIntervalMilliseconds = 33;  // ~30Hz

CurlThread::CurlThread() : action_data_signal_(0) {
  // Normally we would use make_new() here, but this is not a std::unique_ptr
  // and make_new() isn't supported by all targets we build for
  // NOLINTNEXTLINE
  background_thread_.reset(new Thread(ProcessRequests, this));  // NOLINT
}

CurlThread::~CurlThread() {
  CancelAllTransfers();
  ScheduleAction(TransportCurlActionData::Quit());
  background_thread_->Join();
}

void CurlThread::ScheduleAction(const TransportCurlActionData& action_data) {
  MutexLock lock(mutex_);
  action_data_queue_.push_back(action_data);
  action_data_signal_.Post();
}

int CurlThread::CancelRequest(TransportCurl* transport_curl, Response* response,
                              CURL* curl) {
  int removed_from_queue = 0;
  MutexLock lock(mutex_);
  // Remove any actions that are queued for this transfer before scheduling
  // the cancellation.
  auto it = action_data_queue_.begin();
  while (it != action_data_queue_.end()) {
    if (!(it->transport == transport_curl && it->response == response &&
          it->curl == curl)) {
      it++;
      continue;
    }
    switch (it->action) {
      case kRequestedActionPerform:
        removed_from_queue++;
        FIREBASE_CASE_FALLTHROUGH;
      case kRequestedActionPause:
        FIREBASE_CASE_FALLTHROUGH;
      case kRequestedActionCancel:
        FIREBASE_CASE_FALLTHROUGH;
      case kRequestedActionResume:
        action_data_queue_.erase(it);
        it = action_data_queue_.begin();
        break;
      default:
        it++;
        break;
    }
  }
  // Make sure the transfer is currently running.
  bool transferring = false;
  for (auto it = transport_by_response_.begin();
       it != transport_by_response_.end(); ++it) {
    BackgroundTransportCurl* transport = it->second;
    if (it->first == response &&
        transport->transport_curl() == transport_curl &&
        transport->curl() == curl) {
      transferring = true;
    }
  }
  if (transferring) {
    ScheduleAction(
        TransportCurlActionData::Cancel(transport_curl, response, curl));
  }
  return removed_from_queue;
}

bool CurlThread::GetNextAction(TransportCurlActionData* data,
                               int64_t wait_for_milliseconds) {
  if (wait_for_milliseconds) {
    if (wait_for_milliseconds > 0) {
      action_data_signal_.TimedWait(wait_for_milliseconds);
    } else {
      action_data_signal_.Wait();
    }
  } else {
    action_data_signal_.TryWait();
  }
  MutexLock lock(mutex_);
  if (action_data_queue_.empty()) {
    return false;
  }
  assert(data);
  *data = action_data_queue_.front();
  action_data_queue_.pop_front();
  return true;
}

void CurlThread::AddTransfer(BackgroundTransportCurl* transport) {
  MutexLock lock(mutex_);
  assert(transport->response());
  transport_by_response_[transport->response()] = transport;
}

BackgroundTransportCurl* CurlThread::RemoveTransfer(Response* response) {
  MutexLock lock(mutex_);
  auto it = transport_by_response_.find(response);
  if (it == transport_by_response_.end()) return nullptr;
  BackgroundTransportCurl* transport = it->second;
  transport_by_response_.erase(it);
  return transport;
}

void CurlThread::CancelAllTransfers() {
  MutexLock lock(mutex_);
  for (auto it = transport_by_response_.begin();
       it != transport_by_response_.end(); ++it) {
    BackgroundTransportCurl* transport = it->second;
    CancelRequest(transport->transport_curl(), transport->response(),
                  transport->curl());
  }
}

// The libcurl multi interface, which allows for multiple asynchronous
// transfers, requires polling to determine when transfers are complete so
// that the response may be marked completed. The polling and callbacks occur
// in this thread which is started when InitTransportCurl is called.
void CurlThread::ProcessRequests() {
  // Set up multi handle.
  CURLM* curl_multi = curl_multi_init();
  FIREBASE_ASSERT_MESSAGE(curl_multi != nullptr,
                          "curl multi handle failed to initialize");

  int previous_running_handles = 0;
  int expected_running_handles = 0;
  bool quit = false;
  // This will not quit until all transfers either complete or are canceled.
  while (!(quit && expected_running_handles == 0)) {
    int64_t polling_interval = kPollIntervalMilliseconds;
    if (quit || previous_running_handles != expected_running_handles) {
      // If we're quitting or the number of transfers has changed, don't wait.
      polling_interval = 0;
    } else if (expected_running_handles == 0) {
      // If no transfers are active wait indefinitely.
      polling_interval = -1;
    } else {
      // Curl defines the timeout argument as a long which can be a different
      // size per platform so we disable the lint warning about this.
      long timeout_ms = 0;  // NOLINT
      CURLMcode curl_code = curl_multi_timeout(curl_multi, &timeout_ms);
      if (curl_code == CURLM_OK) {
        if (timeout_ms < 0) timeout_ms = kPollIntervalMilliseconds;

        // Convert timeout in milliseconds to timeval.
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        // Wait for curl's file descriptors to signal that data is available.
        fd_set fdread;
        fd_set fdwrite;
        fd_set fdexcep;
        int maxfd = -1;
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
        curl_code =
            curl_multi_fdset(curl_multi, &fdread, &fdwrite, &fdexcep, &maxfd);
        if (curl_code == CURLM_OK && maxfd != -1) {
          polling_interval = 0;
          // TODO(smiles): This should probably also be woken up when items are
          // added to the queue.  If a long delay occurs due to exponential
          // backoff this will hang other transfers for the delay period.
          select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        }
      }
    }

    // Consume new transfer requests.
    TransportCurlActionData action_data;
    while (GetNextAction(&action_data, polling_interval)) {
      polling_interval = 0;
      // Act on the data.
      switch (action_data.action) {
        case kRequestedActionPerform: {
          BackgroundTransportCurl* transport;
          {
            MutexLock lock(mutex_);
            transport = new BackgroundTransportCurl(
                curl_multi, action_data.curl, action_data.request,
                action_data.response, &mutex_, action_data.controller,
                action_data.transport,
                [](BackgroundTransportCurl* background_transport, void* data) {
                  reinterpret_cast<CurlThread*>(data)->RemoveTransfer(
                      background_transport->response());
                },
                this);
          }
          AddTransfer(transport);
          if (transport->PerformBackground(action_data.request)) {
            expected_running_handles++;
          } else {
            delete transport;
          }
          break;
        }
        case kRequestedActionCancel: {
          BackgroundTransportCurl* transport =
              RemoveTransfer(action_data.response);
          if (transport) {
            transport->set_canceled(true);
            delete transport;
            expected_running_handles--;
          }
          break;
        }
        case kRequestedActionPause: {
          MutexLock lock(mutex_);
          auto it = transport_by_response_.find(action_data.response);
          if (it != transport_by_response_.end()) {
            curl_easy_pause(it->second->curl(), CURLPAUSE_ALL);
          }
          break;
        }
        case kRequestedActionResume: {
          MutexLock lock(mutex_);
          auto it = transport_by_response_.find(action_data.response);
          if (it != transport_by_response_.end()) {
            curl_easy_pause(it->second->curl(), CURLPAUSE_CONT);
          }
          break;
        }
        case kRequestedActionQuit: {
          quit = true;
          break;
        }
      }
    }

    // Update controllers with transfer status.
    {
      MutexLock lock(mutex_);
      for (auto it = transport_by_response_.begin();
           it != transport_by_response_.end(); ++it) {
        BackgroundTransportCurl* transport = it->second;
        ControllerCurl* controller = transport->controller();
        if (!controller) continue;
        // Update transfer status.
        CURLINFO transferred_getter;
        CURLINFO size_getter;
        // According to the documentation, CURLINFO_CONTENT_LENGTH_(UP|DOWN)LOAD
        // is deprecated in favor of CURLINFO_CONTENT_LENGTH_(UP|DOWN)LOAD_T,
        // but it looks like our version of libcurl doesn't support the newer _T
        // versions.
        switch (controller->direction()) {
          case kTransferDirectionUpload:
            size_getter = CURLINFO_CONTENT_LENGTH_UPLOAD;
            transferred_getter = CURLINFO_SIZE_UPLOAD;
            static_assert((CURLINFO_CONTENT_LENGTH_UPLOAD &
                           CURLINFO_TYPEMASK) == CURLINFO_DOUBLE,
                          "Unexpected transfer_size type");
            static_assert(
                (CURLINFO_SIZE_UPLOAD & CURLINFO_TYPEMASK) == CURLINFO_DOUBLE,
                "Unexpected bytes_transferred type");
            break;
          case kTransferDirectionDownload:
            size_getter = CURLINFO_CONTENT_LENGTH_DOWNLOAD;
            transferred_getter = CURLINFO_SIZE_DOWNLOAD;
            static_assert((CURLINFO_CONTENT_LENGTH_DOWNLOAD &
                           CURLINFO_TYPEMASK) == CURLINFO_DOUBLE,
                          "Unexpected transfer_size type");
            static_assert(
                (CURLINFO_SIZE_DOWNLOAD & CURLINFO_TYPEMASK) == CURLINFO_DOUBLE,
                "Unexpected bytes_transferred type");
            break;
        }

        CURLcode code;
        double value = 0.0;
        code = curl_easy_getinfo(transport->curl(), size_getter, &value);
        if (code == CURLE_OK) {
          controller->set_transfer_size(static_cast<int64_t>(value));
        }
        value = 0.0;
        code = curl_easy_getinfo(transport->curl(), transferred_getter, &value);
        if (code == CURLE_OK) {
          controller->set_bytes_transferred(static_cast<int64_t>(value));
        }
      }
    }

    int running_handles;
    curl_multi_perform(curl_multi, &running_handles);

    if (expected_running_handles != running_handles) {
      // Some transfers completed.
      int message_count;
      CURLMsg* message;
      while ((message = curl_multi_info_read(curl_multi, &message_count))) {
        switch (message->msg) {
          case CURLMSG_DONE: {
            CURL* handle = message->easy_handle;

            // Get the response object and clean up the easy handle.
            char* char_pointer;
            curl_easy_getinfo(handle, CURLINFO_PRIVATE, &char_pointer);
            curl_multi_remove_handle(curl_multi, handle);
            BackgroundTransportCurl* transport =
                reinterpret_cast<BackgroundTransportCurl*>(char_pointer);

            // Determine if the request timed out.
            if (message->data.result == CURLE_OPERATION_TIMEDOUT) {
              transport->set_timed_out(true);
            }

            // Mark the response complete.
            delete transport;
            expected_running_handles--;
            break;
          }
          default: {
            // Should never happen.
            assert(0);
          }
        }
      }
    }
    previous_running_handles = expected_running_handles;
  }

  // Clean up multi handle before returning.
  curl_multi_cleanup(curl_multi);
}

void CurlThread::ProcessRequests(void* thread) {
  reinterpret_cast<CurlThread*>(thread)->ProcessRequests();
}

void TransportCurl::PerformInternal(
    Request* request, Response* response,
    flatbuffers::unique_ptr<Controller>* controller_out) {
  ControllerCurl* controller =
      controller_out
          ? new ControllerCurl(this,
                               (request->options().method == util::kGet)
                                   ? kTransferDirectionDownload
                                   : kTransferDirectionUpload,
                               response)
          : nullptr;
  if (controller) controller->set_transferring(true);
  {
    MutexLock lock(running_transfers_mutex_);
    running_transfers_++;
  }
  g_curl_thread->ScheduleAction(TransportCurlActionData::Perform(
      this, request, response, reinterpret_cast<CURL*>(curl_), controller));
  if (controller_out) {
    // Normally we would use make_new() here, but this is not a std::unique_ptr
    // and make_new() isn't supported by all targets we build for
    controller_out->reset(controller);  // NOLINT
  }

  // TODO(b/67899466): Remove this once we transition to exclusively
  // asynchronous curl requests.
  if (!is_async_) WaitForAllTransfersToComplete();
}

void TransportCurl::CancelRequest(Response* response) {
  int removed_from_queue = g_curl_thread->CancelRequest(
      this, response, reinterpret_cast<CURL*>(curl_));
  while (removed_from_queue--) SignalTransferComplete();
}

void TransportCurl::PauseRequest(Response* response) {
  g_curl_thread->ScheduleAction(TransportCurlActionData::Pause(
      this, response, reinterpret_cast<CURL*>(curl_)));
}

void TransportCurl::ResumeRequest(Response* response) {
  g_curl_thread->ScheduleAction(TransportCurlActionData::Resume(
      this, response, reinterpret_cast<CURL*>(curl_)));
}

void TransportCurl::SignalTransferComplete() {
  MutexLock lock(running_transfers_mutex_);
  if (running_transfers_) {
    running_transfers_--;
    running_transfers_semaphore_.Post();
  }
}

void TransportCurl::WaitForAllTransfersToComplete() {
  for (;;) {
    bool transfers_complete = false;
    {
      MutexLock lock(running_transfers_mutex_);
      transfers_complete = running_transfers_ == 0;
    }
    if (transfers_complete) break;
    running_transfers_semaphore_.Wait();
  }
}

}  // namespace rest
}  // namespace firebase
