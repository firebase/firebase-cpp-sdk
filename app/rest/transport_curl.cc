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
#include <cstdint>
#include <string>
#include <unordered_map>

#include "app/rest/controller_curl.h"
#include "app/rest/curl/curl_handles.h"
#include "app/rest/curl/curl_io_loop.h"
#include "app/rest/curl/tls_policy.h"
#include "app/rest/curl/transfer.h"
#include "app/rest/util.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "curl/curl.h"

#if FIREBASE_PLATFORM_WINDOWS
#if defined(_MSC_VER)
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "ws2_32.lib")
#endif  // defined(_MSC_VER)
#endif  // FIREBASE_PLATFORM_WINDOWS

namespace firebase {
namespace rest {

// One HTTP request/response pair running on the shared CurlIoLoop.
//
// A RestTransfer is created by TransportCurl::PerformInternal() on the
// caller's thread (so that a Controller handed back to the caller is wired up
// before PerformInternal() returns), but the easy handle is only ever touched
// on the I/O thread: Start(), Cancel(), Pause(), Resume() and OnDone() all run
// there via CurlIoLoop::Post(). The object deletes itself from the I/O thread
// when the transfer finishes, is canceled, or fails to start; its destructor
// completes the Request/Response and releases the owning TransportCurl.
class RestTransfer final : public curl::Transfer {
 public:
  RestTransfer(TransportCurl* transport, CURL* easy, Request* request,
               Response* response, ControllerCurl* controller);
  ~RestTransfer() override;

  // I/O thread. Configures the easy handle and adds it to the loop. If setup
  // fails the response is failed immediately and this object is destroyed.
  void Start();
  // I/O thread. Stops an in-flight transfer, fails the response with
  // HttpNoContent and destroys this object.
  void Cancel();
  // I/O thread.
  void Pause();
  void Resume();
  // Any thread, only when the loop is no longer running (Post() failed):
  // fails the response as canceled and destroys this object.
  void AbortWithoutLoop();

  // curl::Transfer:
  CURL* easy() override { return easy_; }
  void OnDone(CURLcode result) override;

  TransportCurl* transport() const { return transport_; }

 private:
  bool Setup();
  void CheckOk(CURLcode code, const char* msg);
  void CompleteOperation();
  // Unregisters this transfer and destroys it.
  void Finish();

  static size_t HeaderCallback(char* buffer, size_t size, size_t nitems,
                               void* userdata);
  static size_t WriteCallback(char* buffer, size_t size, size_t nmemb,
                              void* userdata);
  static size_t ReadCallback(char* buffer, size_t size, size_t nitems,
                             void* userdata);
  static int ProgressCallback(void* userdata, curl_off_t dltotal,
                              curl_off_t dlnow, curl_off_t ultotal,
                              curl_off_t ulnow);

  TransportCurl* transport_;
  // Owned by transport_ (util::CreateCurlPtr / DestroyCurlPtr); reused by
  // every sequential request on that transport.
  CURL* easy_;
  Request* request_;
  Response* response_;
  // Guarded by g_controller_mutex. Cleared by ~ControllerCurl() if the
  // controller goes away first.
  ControllerCurl* controller_;
  curl::UniqueSList headers_;
  char err_buf_[CURL_ERROR_SIZE];
  // First error reported during setup.
  CURLcode err_code_;
  bool canceled_;
  bool timed_out_;
  // Set when Request::ReadBody() asked to abort the upload, so that the
  // resulting CURLE_ABORTED_BY_CALLBACK is not mistaken for a cancellation.
  bool read_aborted_;
};

namespace {

// Guards RestTransfer::controller_ and the back-pointer ControllerCurl keeps
// to it (ControllerCurl::this_handle_). Intentionally leaked: controllers may
// be destroyed during static destruction.
Mutex* g_controller_mutex = new Mutex();

// Transfers that have been started and not yet finished, keyed by Response so
// that Cancel/Pause/Resume requests (which only know the Response) can find
// them. I/O thread only. Intentionally leaked for the same reason as above.
std::unordered_map<Response*, RestTransfer*>* g_active_transfers =
    new std::unordered_map<Response*, RestTransfer*>();

// I/O thread only.
RestTransfer* FindActiveTransfer(TransportCurl* transport, Response* response) {
  auto it = g_active_transfers->find(response);
  if (it == g_active_transfers->end()) return nullptr;
  // Pointer comparison only: `transport` may already be destroyed.
  if (it->second->transport() != transport) return nullptr;
  return it->second;
}

}  // namespace

void InitTransportCurl() { curl::CurlIoLoop::Acquire(); }

void CleanupTransportCurl() { curl::CurlIoLoop::Release(); }

// ---------------------------------------------------------------------------
// RestTransfer
// ---------------------------------------------------------------------------

RestTransfer::RestTransfer(TransportCurl* transport, CURL* easy,
                           Request* request, Response* response,
                           ControllerCurl* controller)
    : transport_(transport),
      easy_(easy),
      request_(request),
      response_(response),
      controller_(controller),
      headers_(nullptr),
      err_code_(CURLE_OK),
      canceled_(false),
      timed_out_(false),
      read_aborted_(false) {
  assert(transport_);
  assert(request_);
  assert(response_);
  FIREBASE_ASSERT_MESSAGE(easy_ != nullptr,
                          "failed to start a curl easy session");
  err_buf_[0] = '\0';
  if (controller_) {
    controller_->InitializeControllerHandle(&controller_, g_controller_mutex);
  }
}

RestTransfer::~RestTransfer() {
  // Detach from the controller first: after this point the controller can no
  // longer reach us and we can no longer reach it.
  {
    MutexLock lock(*g_controller_mutex);
    if (controller_) {
      controller_->InitializeControllerHandle(nullptr, nullptr);
      controller_->set_transferring(false);
      controller_ = nullptr;
    }
  }

  // The handle is detached from the multi handle by now. Drop the pointers it
  // holds into this object before the header list is freed. This must happen
  // before SignalTransferComplete() below: in asynchronous mode that call may
  // let ~TransportCurl() destroy the handle on another thread.
  curl::SetOpt(easy_, CURLOPT_ERRORBUFFER, static_cast<char*>(nullptr));
  curl::SetOpt(easy_, CURLOPT_HTTPHEADER, static_cast<curl_slist*>(nullptr));
  curl::SetOpt(easy_, CURLOPT_XFERINFODATA, static_cast<void*>(nullptr));
  headers_.reset();

  TransportCurl* transport = transport_;
  transport_ = nullptr;
  if (transport->is_async()) {
    // If this is an asynchronous operation, MarkFailed() or MarkCompleted()
    // could end up attempting to tear down TransportCurl so we signal
    // completion here. Nothing below may touch `transport`.
    transport->SignalTransferComplete();
    CompleteOperation();
  } else {
    // Synchronous operations need all data present in the response before
    // Perform() returns so signal complete after MarkFailed() or
    // MarkCompleted().
    CompleteOperation();
    transport->SignalTransferComplete();
  }
}

void RestTransfer::CompleteOperation() {
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

void RestTransfer::CheckOk(CURLcode code, const char* msg) {
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

size_t RestTransfer::HeaderCallback(char* buffer, size_t size, size_t nitems,
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

size_t RestTransfer::WriteCallback(char* buffer, size_t size, size_t nmemb,
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

size_t RestTransfer::ReadCallback(char* buffer, size_t size, size_t nitems,
                                  void* userdata) {
  FIREBASE_ASSERT_RETURN(0, userdata != nullptr);
  RestTransfer* transfer = static_cast<RestTransfer*>(userdata);
  bool abort = false;
  size_t data_read =
      transfer->request_->ReadBody(buffer, size * nitems, &abort);
  if (abort) {
    transfer->read_aborted_ = true;
    return CURL_READFUNC_ABORT;
  }
  return data_read;
}

int RestTransfer::ProgressCallback(void* userdata, curl_off_t dltotal,
                                   curl_off_t dlnow, curl_off_t ultotal,
                                   curl_off_t ulnow) {
  RestTransfer* transfer = static_cast<RestTransfer*>(userdata);
  if (transfer == nullptr) return 0;
  MutexLock lock(*g_controller_mutex);
  ControllerCurl* controller = transfer->controller_;
  if (controller) {
    switch (controller->direction()) {
      case kTransferDirectionUpload:
        controller->set_transfer_size(static_cast<int64_t>(ultotal));
        controller->set_bytes_transferred(static_cast<int64_t>(ulnow));
        break;
      case kTransferDirectionDownload:
        controller->set_transfer_size(static_cast<int64_t>(dltotal));
        controller->set_bytes_transferred(static_cast<int64_t>(dlnow));
        break;
    }
  }
  return 0;  // Continue the transfer.
}

bool RestTransfer::Setup() {
  RequestOptions& options = request_->options();

  // The easy handle is reused across sequential requests on one TransportCurl;
  // start from libcurl's defaults so nothing from the previous request (custom
  // method, header list, post fields, verbosity, ...) leaks into this one.
  curl_easy_reset(easy_);

  // Set the error buffer before all other options since CheckOk() depends on
  // it.
  err_buf_[0] = '\0';
  CheckOk(curl::SetOpt(easy_, CURLOPT_ERRORBUFFER, err_buf_),
          "set error buffer");

  // Ensure that we are only accepting HTTP(S) traffic.
  // TODO(73540740): Remove support for HTTP.
  CheckOk(curl::SetOpt(easy_, CURLOPT_PROTOCOLS_STR, "http,https"),
          "set valid protocols");

  // Peer/hostname verification, TLS 1.2 minimum, and OS trust store selection
  // are centralized in app/rest/curl/tls_policy.h.
  CheckOk(curl::ApplyTlsPolicy(easy_), "apply TLS policy");

  // Set callback functions.
  CheckOk(curl::SetOpt(easy_, CURLOPT_HEADERFUNCTION, &HeaderCallback),
          "set http header callback");
  CheckOk(
      curl::SetOpt(easy_, CURLOPT_HEADERDATA, static_cast<void*>(response_)),
      "set http header callback data");
  CheckOk(curl::SetOpt(easy_, CURLOPT_WRITEFUNCTION, &WriteCallback),
          "set http body write callback");
  CheckOk(curl::SetOpt(easy_, CURLOPT_WRITEDATA, static_cast<void*>(response_)),
          "set http body write callback data");
  CheckOk(curl::SetOpt(easy_, CURLOPT_READFUNCTION, &ReadCallback),
          "set http body read callback");
  CheckOk(curl::SetOpt(easy_, CURLOPT_READDATA, static_cast<void*>(this)),
          "set http body read callback data");
  CheckOk(curl::SetOpt(easy_, CURLOPT_TIMEOUT_MS,
                       static_cast<long>(options.timeout_ms)),  // NOLINT
          "set http timeout milliseconds");

  // Progress reporting for ControllerCurl. libcurl invokes the callback from
  // within curl_multi_perform() on the I/O thread.
  CheckOk(curl::SetOpt(easy_, CURLOPT_NOPROGRESS, 0L), "enable progress");
  CheckOk(curl::SetOpt(easy_, CURLOPT_XFERINFOFUNCTION, &ProgressCallback),
          "set progress callback");
  CheckOk(curl::SetOpt(easy_, CURLOPT_XFERINFODATA, static_cast<void*>(this)),
          "set progress callback data");

  // Force HTTP/1.1 even when linked against a libcurl build that defaults to
  // HTTP/2 (e.g., system libcurl or internal builds with nghttp2 enabled):
  // Response::ProcessHeader() parses status lines with "HTTP/%*d.%*d %d", which
  // does not match HTTP/2's "HTTP/2 200" status line, and streaming POST
  // uploads rely on HTTP/1.1 "Transfer-Encoding: chunked".
  CheckOk(curl::SetOpt(easy_, CURLOPT_HTTP_VERSION,
                       static_cast<long>(CURL_HTTP_VERSION_1_1)),  // NOLINT
          "set http version to http1");

  // SDK error in initialization stage is not recoverable.
  FIREBASE_ASSERT(err_code_ == CURLE_OK);

  err_code_ = CURLE_OK;

  // Set VERBOSE for debug; does not affect the HTTP connection.
  if (options.verbose) {
    curl::SetOpt(easy_, CURLOPT_VERBOSE, 1L);
  }

  headers_.reset();
  for (const auto& pair : options.header) {
    std::string header;
    header.reserve(pair.first.size() + pair.second.size() + 1);
    header.append(pair.first);
    header.append(1, util::kHttpHeaderSeparator);
    header.append(pair.second);
    curl::SListAppend(&headers_, header.c_str());
  }
  std::string method = util::ToUpper(options.method);
  if (method == util::kPost && options.stream_post_fields) {
    size_t transfer_size = request_->GetPostFieldsSize();
    // If the upload size is unknown use chunked encoding.
    if (transfer_size == ~static_cast<size_t>(0)) {
      // To use POST to a HTTP 1.1 sever we need to use chunked encoding.
      // This is not supported by HTTP 1.0 servers which need to know the size
      // of the request up front.
      curl::SListAppend(&headers_, "Transfer-Encoding: chunked");
    } else {
      // Set the expected POST size, to be compatible with HTTP 1.0 servers.
      curl::SetOpt(easy_, CURLOPT_POSTFIELDSIZE_LARGE,
                   static_cast<curl_off_t>(transfer_size));
      MutexLock lock(*g_controller_mutex);
      if (controller_) {
        controller_->set_transfer_size(static_cast<int64_t>(transfer_size));
      }
    }
  }

  if (headers_ != nullptr) {
    CheckOk(curl::SetOpt(easy_, CURLOPT_HTTPHEADER, headers_.get()),
            "set http header");
  }

  // Set URL
  CheckOk(curl::SetOpt(easy_, CURLOPT_URL, options.url.c_str()),
          "set http url");
  // Set the method.
  if (method == util::kGet) {
    CheckOk(curl::SetOpt(easy_, CURLOPT_HTTPGET, 1L), "set http method");
  } else if (method == util::kPost) {
    CheckOk(curl::SetOpt(easy_, CURLOPT_POST, 1L), "set http method");
  } else {
    // For all other method. Note: If your code is depending on a documented
    // method e.g. PUT, try add another else-if branch above instead of relying
    // on this generic mechanism.
    CheckOk(curl::SetOpt(easy_, CURLOPT_CUSTOMREQUEST, method.c_str()),
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
    // CURLOPT_POSTFIELDS does not copy: options.post_fields (owned by the
    // Request, which outlives the transfer) must stay unchanged until done.
    CheckOk(curl::SetOpt(easy_, CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(options.post_fields.size())),
            "set http post fields");
    CheckOk(
        curl::SetOpt(easy_, CURLOPT_POSTFIELDS, options.post_fields.c_str()),
        "set http post fields");
    MutexLock lock(*g_controller_mutex);
    if (controller_) {
      controller_->set_transfer_size(
          static_cast<int64_t>(options.post_fields.size()));
    }
  }

  return err_code_ == CURLE_OK;
}

void RestTransfer::Start() {
  (*g_active_transfers)[response_] = this;
  if (Setup()) {
    CURLMcode code = curl::CurlIoLoop::Get().Add(*this);
    if (code == CURLM_OK) return;  // OnDone() will finish the transfer.
    LogError("failed to start transfer to %s: %s",
             request_->options().url.c_str(), curl_multi_strerror(code));
    err_code_ = CURLE_FAILED_INIT;
  }
  // If any error happens during the setup, we do not perform the http request
  // and fail the response immediately instead.
  response_->set_sdk_error_code(err_code_);
  canceled_ = true;
  Finish();
}

void RestTransfer::Cancel() {
  curl::CurlIoLoop::Get().Remove(*this);
  canceled_ = true;
  Finish();
}

void RestTransfer::Pause() { curl_easy_pause(easy_, CURLPAUSE_ALL); }

void RestTransfer::Resume() { curl_easy_pause(easy_, CURLPAUSE_CONT); }

void RestTransfer::AbortWithoutLoop() {
  canceled_ = true;
  // Not registered in g_active_transfers (Start() never ran), so just
  // complete and destroy.
  delete this;
}

void RestTransfer::OnDone(CURLcode result) {
  if (result == CURLE_OPERATION_TIMEDOUT) {
    timed_out_ = true;
  } else if (result == CURLE_ABORTED_BY_CALLBACK && !read_aborted_) {
    // The I/O loop is shutting down with this transfer still in flight.
    canceled_ = true;
  }
  if (result != CURLE_OK) {
    LogDebug("transfer to %s finished with curl error (%d) %s: %s",
             request_->options().url.c_str(), static_cast<int>(result),
             curl_easy_strerror(result), err_buf_);
  }
  Finish();
}

void RestTransfer::Finish() {
  g_active_transfers->erase(response_);
  delete this;
}

// ---------------------------------------------------------------------------
// TransportCurl
// ---------------------------------------------------------------------------

TransportCurl::TransportCurl()
    : is_async_(false), running_transfers_(0), running_transfers_semaphore_(0) {
  curl_ = util::CreateCurlPtr();
  assert(curl_ != nullptr);  // Failed to get curl pointer.  Something is wrong.
}

TransportCurl::~TransportCurl() {
  WaitForAllTransfersToComplete();
  util::DestroyCurlPtr(curl_);
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
  // Wires up the controller now so that Controller::Cancel() etc. work even
  // before the I/O thread has picked the transfer up.
  RestTransfer* transfer = new RestTransfer(this, static_cast<CURL*>(curl_),
                                            request, response, controller);
  if (!curl::CurlIoLoop::Get().Post([transfer]() { transfer->Start(); })) {
    // The loop has already stopped (Perform() after CleanupTransportCurl()).
    // Fail the request instead of leaving the caller waiting forever.
    transfer->AbortWithoutLoop();
  }
  if (controller_out) {
    // Normally we would use make_new() here, but this is not a std::unique_ptr
    // and make_new() isn't supported by all targets we build for
    controller_out->reset(controller);  // NOLINT
  }

  // TODO(b/67899466): Remove this once we transition to exclusively
  // asynchronous curl requests.
  if (!is_async_) {
    // A synchronous Perform() from the I/O thread itself (e.g. from inside a
    // Response::MarkCompleted() override) can never complete: the thread that
    // would run the transfer is the one blocked here.
    if (curl::CurlIoLoop::IsRunning() &&
        curl::CurlIoLoop::Get().IsOnIoThread()) {
      LogError(
          "Synchronous TransportCurl::Perform() called on the curl I/O thread; "
          "this will deadlock. Use set_is_async(true) or move the call to "
          "another thread.");
      FIREBASE_DEV_ASSERT_MESSAGE(false,
                                  "Synchronous TransportCurl::Perform() on "
                                  "the curl I/O thread");
    }
    WaitForAllTransfersToComplete();
  }
}

void TransportCurl::CancelRequest(Response* response) {
  TransportCurl* transport = this;
  curl::CurlIoLoop::Get().Post([transport, response]() {
    RestTransfer* transfer = FindActiveTransfer(transport, response);
    if (transfer) transfer->Cancel();
  });
}

void TransportCurl::PauseRequest(Response* response) {
  TransportCurl* transport = this;
  curl::CurlIoLoop::Get().Post([transport, response]() {
    RestTransfer* transfer = FindActiveTransfer(transport, response);
    if (transfer) transfer->Pause();
  });
}

void TransportCurl::ResumeRequest(Response* response) {
  TransportCurl* transport = this;
  curl::CurlIoLoop::Get().Post([transport, response]() {
    RestTransfer* transfer = FindActiveTransfer(transport, response);
    if (transfer) transfer->Resume();
  });
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
