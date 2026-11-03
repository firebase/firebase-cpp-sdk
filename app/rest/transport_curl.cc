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

#include <atomic>
#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "app/rest/controller_curl.h"
#include "app/rest/curl/curl_global.h"
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
  RestTransfer(TransportCurl* transport, uint64_t transfer_id, bool is_async,
               Request* request, Response* response,
               std::shared_ptr<ControllerCurl::TransferControl> control);
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
  // Any thread, only when the loop is no longer running (PostIfRunning()
  // failed): fails the response as canceled and destroys this object.
  void AbortWithoutLoop();

  // curl::Transfer:
  CURL* easy() override { return easy_.get(); }
  void OnDone(CURLcode result) override;

  TransportCurl* transport() const { return transport_; }
  uint64_t transfer_id() const { return transfer_id_; }

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
  uint64_t transfer_id_;
  bool is_async_;
  Request* request_;
  Response* response_;
  std::shared_ptr<ControllerCurl::TransferControl> control_;
  curl::UniqueSList headers_;
  char err_buf_[CURL_ERROR_SIZE];
  // Holds a CurlGlobal reference before initializing easy_ so curl_easy_init()
  // is always called after curl_global_init(), and each transfer owns an
  // independent easy handle (connection/TLS/DNS/HTTP2 caches are shared on
  // CurlIoLoop::multi()). Declared after headers_ and err_buf_ so reverse
  // destruction order destroys easy_ before the buffers it references.
  curl::CurlGlobalRef global_ref_;
  curl::UniqueEasy easy_;
  // First error reported during setup.
  CURLcode err_code_;
  bool canceled_;
  bool timed_out_;
  // Set when Request::ReadBody() asked to abort the upload, so that the
  // resulting CURLE_ABORTED_BY_CALLBACK is not mistaken for a cancellation.
  bool read_aborted_;
};

namespace {

// Monotonically increasing ID assigned to each transfer so that a delayed
// Cancel/Pause/Resume on a completed transfer can never hit a new transfer
// that reuses the same Response* address (ABA race).
std::atomic<uint64_t> g_next_transfer_id{1};

// Transfers that have been started and not yet finished, keyed by transfer_id
// so that Cancel/Pause/Resume requests can find them without ABA hazards.
// I/O thread only. Intentionally leaked for static destruction safety.
std::unordered_map<uint64_t, RestTransfer*>* g_active_transfers =
    new std::unordered_map<uint64_t, RestTransfer*>();

// I/O thread only.
RestTransfer* FindActiveTransfer(TransportCurl* transport,
                                 uint64_t transfer_id) {
  auto it = g_active_transfers->find(transfer_id);
  if (it == g_active_transfers->end()) return nullptr;
  // Pointer comparison only: `transport` may already be destroyed.
  if (it->second->transport() != transport) return nullptr;
  return it->second;
}

// True when the linked libcurl build includes HTTP/2 (nghttp2) support.
// Bundled SDK builds set USE_NGHTTP2=OFF to avoid an extra external dependency,
// while system-libcurl builds on Linux/macOS typically enable HTTP/2.
bool LibcurlSupportsHttp2() {
  static const bool kSupported = []() {
    const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
    return info != nullptr && (info->features & CURL_VERSION_HTTP2) != 0;
  }();
  return kSupported;
}

}  // namespace

void InitTransportCurl() { curl::CurlIoLoop::Acquire(); }

void CleanupTransportCurl() { curl::CurlIoLoop::Release(); }

// ---------------------------------------------------------------------------
// RestTransfer
// ---------------------------------------------------------------------------

RestTransfer::RestTransfer(
    TransportCurl* transport, uint64_t transfer_id, bool is_async,
    Request* request, Response* response,
    std::shared_ptr<ControllerCurl::TransferControl> control)
    : transport_(transport),
      transfer_id_(transfer_id),
      is_async_(is_async),
      request_(request),
      response_(response),
      control_(std::move(control)),
      headers_(nullptr),
      global_ref_(),
      easy_(curl_easy_init()),
      err_code_(CURLE_OK),
      canceled_(false),
      timed_out_(false),
      read_aborted_(false) {
  assert(transport_);
  assert(request_);
  assert(response_);
  err_buf_[0] = '\0';
}

RestTransfer::~RestTransfer() {
  // Detach from the shared controller state first: after this point the
  // controller knows the transfer is no longer active.
  if (control_) {
    {
      MutexLock lock(control_->mutex);
      control_->transferring.store(false, std::memory_order_release);
      control_->is_paused.store(false, std::memory_order_release);
      control_->transport = nullptr;
    }
    control_.reset();
  }

  // The handle is detached from the multi handle by now. Destroy the easy
  // handle and header list before SignalTransferComplete() below.
  if (easy_) {
    curl::SetOpt(easy_.get(), CURLOPT_ERRORBUFFER, static_cast<char*>(nullptr));
    curl::SetOpt(easy_.get(), CURLOPT_HTTPHEADER,
                 static_cast<curl_slist*>(nullptr));
    curl::SetOpt(easy_.get(), CURLOPT_XFERINFODATA,
                 static_cast<void*>(nullptr));
    easy_.reset();
  }
  headers_.reset();

  TransportCurl* transport = transport_;
  transport_ = nullptr;
  if (is_async_) {
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
  ControllerCurl::TransferControl* control = transfer->control_.get();
  if (control) {
    switch (control->direction) {
      case kTransferDirectionUpload:
        if (ultotal > 0) {
          ControllerCurl::UpdateTransferSize(control,
                                             static_cast<int64_t>(ultotal));
        }
        ControllerCurl::UpdateBytesTransferred(control,
                                               static_cast<int64_t>(ulnow));
        break;
      case kTransferDirectionDownload:
        if (dltotal > 0) {
          ControllerCurl::UpdateTransferSize(control,
                                             static_cast<int64_t>(dltotal));
        }
        ControllerCurl::UpdateBytesTransferred(control,
                                               static_cast<int64_t>(dlnow));
        break;
    }
  }
  return 0;  // Continue the transfer.
}

bool RestTransfer::Setup() {
  if (!easy_) {
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
  CheckOk(
      curl::SetOpt(easy, CURLOPT_HEADERDATA, static_cast<void*>(response_)),
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
  if (control_ != nullptr) {
    CheckOk(curl::SetOpt(easy, CURLOPT_NOPROGRESS, 0L), "enable progress");
    CheckOk(curl::SetOpt(easy, CURLOPT_XFERINFOFUNCTION, &ProgressCallback),
            "set progress callback");
    CheckOk(curl::SetOpt(easy, CURLOPT_XFERINFODATA, static_cast<void*>(this)),
            "set progress callback data");
  }

  std::string method = util::ToUpper(options.method);
  const bool is_streamed_upload =
      options.stream_post_fields &&
      (method == util::kPost ||
       (method != util::kGet && request_->GetPostFieldsSize() > 0));
  const bool stream_unknown_post_size =
      (is_streamed_upload &&
       request_->GetPostFieldsSize() == ~static_cast<size_t>(0));
  bool has_transfer_encoding_header = stream_unknown_post_size;
  for (const auto& pair : options.header) {
    if (util::ToLower(pair.first) == "transfer-encoding") {
      has_transfer_encoding_header = true;
      break;
    }
  }

  // Negotiate HTTP/2 over TLS (ALPN) when libcurl is built with HTTP/2 support,
  // unless the request relies on an HTTP/1.1 Transfer-Encoding header
  // (forbidden by RFC 9113 section 8.2.2). Plain HTTP and servers without h2
  // fall back to HTTP/1.1 automatically.
  const long http_version =  // NOLINT
      (!has_transfer_encoding_header && LibcurlSupportsHttp2())
          ? static_cast<long>(CURL_HTTP_VERSION_2TLS)  // NOLINT
          : static_cast<long>(CURL_HTTP_VERSION_1_1);  // NOLINT
  CheckOk(curl::SetOpt(easy, CURLOPT_HTTP_VERSION, http_version),
          "set http version");

  if (err_code_ != CURLE_OK) {
    return false;
  }

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
    if (!curl::SListAppend(&headers_, header.c_str())) {
      CheckOk(CURLE_OUT_OF_MEMORY, "append http header");
    }
  }
  if (is_streamed_upload) {
    size_t transfer_size = request_->GetPostFieldsSize();
    if (method != util::kPost) {
      CheckOk(curl::SetOpt(easy, CURLOPT_UPLOAD, 1L), "set http upload");
    }
    // If the upload size is unknown use chunked encoding.
    if (stream_unknown_post_size) {
      // To use POST/PUT to a HTTP 1.1 server we need to use chunked encoding.
      // This is not supported by HTTP 1.0 servers which need to know the size
      // of the request up front.
      if (!curl::SListAppend(&headers_, "Transfer-Encoding: chunked")) {
        CheckOk(CURLE_OUT_OF_MEMORY, "append chunked header");
      }
    } else {
      // Set the expected upload size, to be compatible with HTTP 1.0 servers.
      const CURLoption size_opt = (method == util::kPost)
                                      ? CURLOPT_POSTFIELDSIZE_LARGE
                                      : CURLOPT_INFILESIZE_LARGE;
      CheckOk(
          curl::SetOpt(easy, size_opt, static_cast<curl_off_t>(transfer_size)),
          "set http upload size");
      ControllerCurl::UpdateTransferSize(control_.get(),
                                         static_cast<int64_t>(transfer_size));
    }
  }

  if (headers_ != nullptr) {
    CheckOk(curl::SetOpt(easy, CURLOPT_HTTPHEADER, headers_.get()),
            "set http header");
  }

  // Set URL
  CheckOk(curl::SetOpt(easy, CURLOPT_URL, options.url.c_str()),
          "set http url");
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
    ControllerCurl::UpdateTransferSize(
        control_.get(), static_cast<int64_t>(options.post_fields.size()));
  }

  return err_code_ == CURLE_OK;
}

void RestTransfer::Start() {
  (*g_active_transfers)[transfer_id_] = this;
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
  if (easy_) {
    curl::CurlIoLoop::Get().Remove(*this);
  }
  canceled_ = true;
  Finish();
}

void RestTransfer::Pause() {
  if (easy_) {
    curl_easy_pause(easy_.get(), CURLPAUSE_ALL);
  }
}

void RestTransfer::Resume() {
  if (easy_) {
    curl_easy_pause(easy_.get(), CURLPAUSE_CONT);
  }
}

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
    response_->set_sdk_error_code(result);
    LogDebug("transfer to %s finished with curl error (%d) %s: %s",
             request_->options().url.c_str(), static_cast<int>(result),
             curl_easy_strerror(result), err_buf_);
  }
  Finish();
}

void RestTransfer::Finish() {
  g_active_transfers->erase(transfer_id_);
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
  const uint64_t transfer_id =
      g_next_transfer_id.fetch_add(1, std::memory_order_relaxed);
  const bool is_async = is_async_.load();
  const bool is_get = util::ToUpper(request->options().method) == util::kGet;
  ControllerCurl* controller =
      controller_out
          ? new ControllerCurl(this,
                               is_get ? kTransferDirectionDownload
                                      : kTransferDirectionUpload,
                               response, transfer_id)
          : nullptr;
  std::shared_ptr<ControllerCurl::TransferControl> control;
  if (controller) {
    controller->set_transferring(true);
    control = controller->control();
    // Populate *controller_out before posting to the I/O loop (or calling
    // AbortWithoutLoop()) so completion callbacks always see *controller_out
    // populated and we never touch caller memory after an async completion.
    controller_out->reset(controller);  // NOLINT
  }
  {
    MutexLock lock(running_transfers_mutex_);
    running_transfers_++;
  }
  RestTransfer* transfer = new RestTransfer(
      this, transfer_id, is_async, request, response, std::move(control));
  if (!curl::CurlIoLoop::PostIfRunning(
          [transfer]() { transfer->Start(); })) {
    // The loop has already stopped (Perform() after CleanupTransportCurl()).
    // Fail the request instead of leaving the caller waiting forever.
    transfer->AbortWithoutLoop();
  }

  // TODO(b/67899466): Remove this once we transition to exclusively
  // asynchronous curl requests.
  if (!is_async) {
    // A synchronous Perform() from the I/O thread itself (e.g. from inside a
    // Response::MarkCompleted() override) can never complete: the thread that
    // would run the transfer is the one blocked here.
    if (curl::CurlIoLoop::IsOnIoThreadCurrent()) {
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

void TransportCurl::CancelRequestById(TransportCurl* transport,
                                      uint64_t transfer_id) {
  curl::CurlIoLoop::PostIfRunning([transport, transfer_id]() {
    RestTransfer* transfer = FindActiveTransfer(transport, transfer_id);
    if (transfer) transfer->Cancel();
  });
}

void TransportCurl::CancelRequest(uint64_t transfer_id) {
  CancelRequestById(this, transfer_id);
}

void TransportCurl::PauseRequestById(TransportCurl* transport,
                                     uint64_t transfer_id) {
  curl::CurlIoLoop::PostIfRunning([transport, transfer_id]() {
    RestTransfer* transfer = FindActiveTransfer(transport, transfer_id);
    if (transfer) transfer->Pause();
  });
}

void TransportCurl::PauseRequest(uint64_t transfer_id) {
  PauseRequestById(this, transfer_id);
}

void TransportCurl::ResumeRequestById(TransportCurl* transport,
                                      uint64_t transfer_id) {
  curl::CurlIoLoop::PostIfRunning([transport, transfer_id]() {
    RestTransfer* transfer = FindActiveTransfer(transport, transfer_id);
    if (transfer) transfer->Resume();
  });
}

void TransportCurl::ResumeRequest(uint64_t transfer_id) {
  ResumeRequestById(this, transfer_id);
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
    {
      MutexLock lock(running_transfers_mutex_);
      if (running_transfers_ == 0) break;
    }
    running_transfers_semaphore_.Wait();
    {
      MutexLock lock(running_transfers_mutex_);
      if (running_transfers_ == 0) {
        // Re-post so any additional concurrent waiters also wake and observe
        // running_transfers_ == 0 instead of blocking forever.
        running_transfers_semaphore_.Post();
        break;
      }
    }
  }
}

}  // namespace rest
}  // namespace firebase
