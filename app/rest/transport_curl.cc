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
#include <memory>
#include <string>
#include <utility>

#include "app/rest/controller_curl.h"
#include "app/rest/curl/curl_handles.h"
#include "app/rest/curl/curl_io_loop.h"
#include "app/rest/curl/tls_policy.h"
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
// caller's thread (so that any Controller handed back to the caller is wired up
// before PerformInternal() returns), while its CURL easy handle is created,
// configured, driven, and destroyed exclusively on the I/O thread. Connection
// reuse, HTTP/2 stream multiplexing, TLS session caching, and DNS caching all
// live on CurlIoLoop's shared CURLM multi handle, so fresh per-transfer easy
// handles retain full pooling with zero cross-request option bleed.
class RestTransfer final : public curl::Transfer {
 public:
  RestTransfer(TransportCurl* transport, Request* request, Response* response,
               std::shared_ptr<TransferControl> control);
  ~RestTransfer() override;

  // I/O thread. Configures a fresh easy handle and adds it to the loop. If
  // setup fails or the transfer was already canceled, fails immediately and
  // destroys this object.
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
  CURL* easy() override { return easy_.get(); }
  void OnDone(CURLcode result) override;

 private:
  bool Setup();
  void CheckOk(CURLcode code, const char* msg);
  void CompleteOperation();

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
  Request* request_;
  Response* response_;
  std::shared_ptr<TransferControl> control_;
  curl::UniqueSList headers_;
  char err_buf_[CURL_ERROR_SIZE];
  // Declared after headers_ and err_buf_ so any implicit destruction also tears
  // down easy_ before the buffers it references.
  curl::UniqueEasy easy_;
  // First error reported during setup.
  CURLcode err_code_;
  bool canceled_;
  bool timed_out_;
  // Set when Request::ReadBody() asked to abort the upload, so that the
  // resulting CURLE_ABORTED_BY_CALLBACK is not mistaken for a cancellation.
  bool read_aborted_;
};

void TransferControl::PauseOnIoThread(RestTransfer* transfer) {
  transfer->Pause();
}

void TransferControl::ResumeOnIoThread(RestTransfer* transfer) {
  transfer->Resume();
}

void TransferControl::CancelOnIoThread(RestTransfer* transfer) {
  transfer->Cancel();
}

void InitTransportCurl() { curl::CurlIoLoop::Acquire(); }

void CleanupTransportCurl() { curl::CurlIoLoop::Release(); }

// ---------------------------------------------------------------------------
// RestTransfer
// ---------------------------------------------------------------------------

RestTransfer::RestTransfer(TransportCurl* transport, Request* request,
                           Response* response,
                           std::shared_ptr<TransferControl> control)
    : transport_(transport),
      request_(request),
      response_(response),
      control_(std::move(control)),
      headers_(nullptr),
      easy_(nullptr),
      err_code_(CURLE_OK),
      canceled_(false),
      timed_out_(false),
      read_aborted_(false) {
  assert(transport_);
  assert(request_);
  assert(response_);
  err_buf_[0] = '\0';
  if (control_) {
    MutexLock lock(control_->mutex);
    control_->transfer = this;
  }
}

RestTransfer::~RestTransfer() {
  if (control_) {
    MutexLock lock(control_->mutex);
    control_->transfer = nullptr;
    control_->transferring = false;
    control_->paused = false;
  }

  // Destroy the easy handle (already detached from CURLM) on the I/O thread
  // before freeing headers_ or signaling TransportCurl.
  easy_.reset();
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
  if (transfer == nullptr || !transfer->control_) return 0;
  switch (transfer->control_->direction) {
    case kTransferDirectionUpload:
      transfer->control_->SetTransferSize(static_cast<int64_t>(ultotal));
      transfer->control_->SetBytesTransferred(static_cast<int64_t>(ulnow));
      break;
    case kTransferDirectionDownload:
      transfer->control_->SetTransferSize(static_cast<int64_t>(dltotal));
      transfer->control_->SetBytesTransferred(static_cast<int64_t>(dlnow));
      break;
  }
  return 0;  // Continue the transfer.
}

bool RestTransfer::Setup() {
  easy_.reset(curl_easy_init());
  if (easy_ == nullptr) {
    LogError("failed to initialize curl easy handle");
    err_code_ = CURLE_FAILED_INIT;
    return false;
  }
  CURL* easy = easy_.get();
  RequestOptions& options = request_->options();

  // Set the error buffer before all other options since CheckOk() depends on
  // it.
  err_buf_[0] = '\0';
  CheckOk(curl::SetOpt(easy, CURLOPT_ERRORBUFFER, err_buf_),
          "set error buffer");

  // Ensure that we are only accepting HTTP(S) traffic.
  // TODO(73540740): Remove support for HTTP.
  CheckOk(curl::SetOpt(easy, CURLOPT_PROTOCOLS_STR, "http,https"),
          "set valid protocols");

  // Peer/hostname verification, TLS 1.2 minimum, and OS trust store selection
  // are centralized in app/rest/curl/tls_policy.h.
  CheckOk(curl::ApplyTlsPolicy(easy), "apply TLS policy");

  // Set callback functions.
  CheckOk(curl::SetOpt(easy, CURLOPT_HEADERFUNCTION, &HeaderCallback),
          "set http header callback");
  CheckOk(curl::SetOpt(easy, CURLOPT_HEADERDATA, static_cast<void*>(response_)),
          "set http header callback data");
  CheckOk(curl::SetOpt(easy, CURLOPT_WRITEFUNCTION, &WriteCallback),
          "set http body write callback");
  CheckOk(curl::SetOpt(easy, CURLOPT_WRITEDATA, static_cast<void*>(response_)),
          "set http body write callback data");
  CheckOk(curl::SetOpt(easy, CURLOPT_READFUNCTION, &ReadCallback),
          "set http body read callback");
  CheckOk(curl::SetOpt(easy, CURLOPT_READDATA, static_cast<void*>(this)),
          "set http body read callback data");
  CheckOk(curl::SetOpt(easy, CURLOPT_TIMEOUT_MS,
                       static_cast<long>(options.timeout_ms)),  // NOLINT
          "set http timeout milliseconds");

  // Progress reporting for ControllerCurl. libcurl invokes the callback from
  // within curl_multi_perform() on the I/O thread.
  if (control_) {
    CheckOk(curl::SetOpt(easy, CURLOPT_NOPROGRESS, 0L), "enable progress");
    CheckOk(curl::SetOpt(easy, CURLOPT_XFERINFOFUNCTION, &ProgressCallback),
            "set progress callback");
    CheckOk(curl::SetOpt(easy, CURLOPT_XFERINFODATA, static_cast<void*>(this)),
            "set progress callback data");
  }

  // Force HTTP/1.1 until Response::ProcessHeader() supports HTTP/2 status
  // lines and lowercase headers.
  CheckOk(curl::SetOpt(easy, CURLOPT_HTTP_VERSION,
                       static_cast<long>(CURL_HTTP_VERSION_1_1)),  // NOLINT
          "set http version to http1");

  // SDK error in initialization stage is not recoverable.
  FIREBASE_ASSERT(err_code_ == CURLE_OK);

  err_code_ = CURLE_OK;

  // Set VERBOSE for debug; does not affect the HTTP connection.
  if (options.verbose) {
    curl::SetOpt(easy, CURLOPT_VERBOSE, 1L);
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
      curl::SetOpt(easy, CURLOPT_POSTFIELDSIZE_LARGE,
                   static_cast<curl_off_t>(transfer_size));
      if (control_) {
        control_->SetTransferSize(static_cast<int64_t>(transfer_size));
      }
    }
  }

  if (headers_ != nullptr) {
    CheckOk(curl::SetOpt(easy, CURLOPT_HTTPHEADER, headers_.get()),
            "set http header");
  }

  // Set URL
  CheckOk(curl::SetOpt(easy, CURLOPT_URL, options.url.c_str()), "set http url");
  // Set the method.
  if (method == util::kGet) {
    CheckOk(curl::SetOpt(easy, CURLOPT_HTTPGET, 1L), "set http method");
  } else if (method == util::kPost) {
    CheckOk(curl::SetOpt(easy, CURLOPT_POST, 1L), "set http method");
  } else {
    // For all other method. Note: If your code is depending on a documented
    // method e.g. PUT, try add another else-if branch above instead of relying
    // on this generic mechanism.
    CheckOk(curl::SetOpt(easy, CURLOPT_CUSTOMREQUEST, method.c_str()),
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
    CheckOk(curl::SetOpt(easy, CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(options.post_fields.size())),
            "set http post fields");
    CheckOk(
        curl::SetOpt(easy, CURLOPT_POSTFIELDS, options.post_fields.c_str()),
        "set http post fields");
    if (control_) {
      control_->SetTransferSize(
          static_cast<int64_t>(options.post_fields.size()));
    }
  }

  return err_code_ == CURLE_OK;
}

void RestTransfer::Start() {
  if (control_) {
    bool already_canceled = false;
    {
      MutexLock lock(control_->mutex);
      already_canceled = control_->canceled;
    }
    if (already_canceled) {
      canceled_ = true;
      delete this;
      return;
    }
  }
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
  delete this;
}

void RestTransfer::Cancel() {
  curl::CurlIoLoop::Get().Remove(*this);
  canceled_ = true;
  delete this;
}

void RestTransfer::Pause() {
  if (easy_ != nullptr) curl_easy_pause(easy_.get(), CURLPAUSE_ALL);
}

void RestTransfer::Resume() {
  if (easy_ != nullptr) curl_easy_pause(easy_.get(), CURLPAUSE_CONT);
}

void RestTransfer::AbortWithoutLoop() {
  canceled_ = true;
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
  delete this;
}

// ---------------------------------------------------------------------------
// TransportCurl
// ---------------------------------------------------------------------------

TransportCurl::TransportCurl()
    : is_async_(false), running_transfers_(0), running_transfers_semaphore_(0) {}

TransportCurl::~TransportCurl() { WaitForAllTransfersToComplete(); }

void TransportCurl::PerformInternal(
    Request* request, Response* response,
    flatbuffers::unique_ptr<Controller>* controller_out) {
  std::shared_ptr<TransferControl> control;
  if (controller_out) {
    TransferDirection direction = (request->options().method == util::kGet)
                                      ? kTransferDirectionDownload
                                      : kTransferDirectionUpload;
    control = std::make_shared<TransferControl>(direction);
  }
  {
    MutexLock lock(running_transfers_mutex_);
    running_transfers_++;
  }
  // Wires up TransferControl now so that Controller::Cancel() etc. work even
  // before the I/O thread has picked the transfer up.
  RestTransfer* transfer = new RestTransfer(this, request, response, control);
  if (!curl::CurlIoLoop::PostIfRunning([transfer]() { transfer->Start(); })) {
    // The loop is not running (Perform() before InitTransportCurl() or
    // after/racing with CleanupTransportCurl()). Fail the request instead of
    // asserting or leaving the caller waiting forever.
    transfer->AbortWithoutLoop();
  }
  if (controller_out) {
    // Normally we would use make_new() here, but this is not a std::unique_ptr
    // and make_new() isn't supported by all targets we build for
    controller_out->reset(new ControllerCurl(std::move(control)));  // NOLINT
  }

  // TODO(b/67899466): Remove this once we transition to exclusively
  // asynchronous curl requests.
  if (!is_async_) {
    // A synchronous Perform() from the I/O thread itself (e.g. from inside a
    // Response::MarkCompleted() override) can never complete: the thread that
    // would run the transfer is the one blocked here.
    if (curl::CurlIoLoop::IsCurrentThreadIoThread()) {
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
