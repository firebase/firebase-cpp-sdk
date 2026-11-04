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
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "app/rest/controller_curl.h"
#include "app/rest/curl/curl_io_loop.h"
#include "app/rest/curl/tls_policy.h"
#include "app/rest/util.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"
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

namespace {

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

// A single FIFO worker thread on which asynchronous transfers deliver
// Response::MarkCompleted() / MarkFailed().
//
// The curl I/O thread is shared by every REST transfer in the process, so
// product code (future completion, user callbacks, Storage file writes) must
// not run on it: a callback that blocks on another request would otherwise
// deadlock all networking. Completions are therefore handed to this queue.
// Unlike scheduler::Scheduler, Stop() runs every queued completion before
// joining, which is what lets CleanupTransportCurl() promise that in-flight
// transfers are failed before it returns.
class CompletionQueue {
 public:
  CompletionQueue()
      : mutex_(Mutex::kModeNonRecursive),
        wakeup_(0),
        stopped_(false),
        thread_(ThreadMain, this) {}

  ~CompletionQueue() { Stop(); }

  // Thread-safe. Returns false (without queuing) once Stop() has begun.
  bool Post(std::function<void()> fn) {
    {
      MutexLock lock(mutex_);
      if (stopped_) return false;
      queue_.push_back(std::move(fn));
    }
    wakeup_.Post();
    return true;
  }

  // Runs everything queued so far, rejects further posts and joins the
  // worker. Must not be called from the worker thread itself.
  void Stop() {
    {
      MutexLock lock(mutex_);
      if (stopped_) return;
      stopped_ = true;
    }
    wakeup_.Post();
    thread_.Join();
  }

 private:
  static void ThreadMain(void* arg) {
    static_cast<CompletionQueue*>(arg)->Run();
  }

  void Run() {
    for (;;) {
      std::deque<std::function<void()>> batch;
      bool stop = false;
      {
        MutexLock lock(mutex_);
        batch.swap(queue_);
        stop = stopped_;
      }
      for (std::function<void()>& fn : batch) fn();
      if (stop) {
        // Posts are rejected once stopped_ is set, so the queue is final.
        MutexLock lock(mutex_);
        if (queue_.empty()) return;
        continue;
      }
      wakeup_.Wait();
    }
  }

  Mutex mutex_;
  Semaphore wakeup_;
  bool stopped_;
  std::deque<std::function<void()>> queue_;
  Thread thread_;
};

// Reference-counted alongside CurlIoLoop by InitTransportCurl() /
// CleanupTransportCurl(). The mutex is leaked on purpose so it survives static
// destruction, matching the other process-wide mutexes in app/rest.
Mutex* g_completion_mutex = new Mutex(Mutex::kModeNonRecursive);
int g_completion_ref_count = 0;
CompletionQueue* g_completion_queue = nullptr;

// Hands fn to the completion thread. Returns false if no queue exists (the
// transport is not initialized), in which case the caller runs fn inline.
bool PostCompletion(std::function<void()> fn) {
  MutexLock lock(*g_completion_mutex);
  return g_completion_queue != nullptr &&
         g_completion_queue->Post(std::move(fn));
}

}  // namespace

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
  // Builds the completion step (MarkCompleted / MarkFailed on request_ and
  // response_) for the current canceled_/timed_out_ state. The returned
  // function does not touch this RestTransfer.
  std::function<void()> MakeCompletion() const;

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
  // Sticky pause flag (I/O thread only). curl_easy_pause() only takes effect
  // once libcurl has a connection for the handle; before that (DNS, TCP, TLS,
  // or before Start() ran) it returns CURLE_BAD_FUNCTION_ARGUMENT and records
  // nothing. The data callbacks consult this flag and return
  // CURL_*FUNC_PAUSE so a Pause() issued at any point really stops the
  // transfer at the first byte.
  bool paused_;
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

void InitTransportCurl() {
  curl::CurlIoLoop::Acquire();
  MutexLock lock(*g_completion_mutex);
  if (g_completion_ref_count++ == 0) {
    FIREBASE_ASSERT(g_completion_queue == nullptr);
    g_completion_queue = new CompletionQueue();
  }
}

void CleanupTransportCurl() {
  // Stopping the loop fails every in-flight transfer; their completions are
  // queued on the completion thread, which is drained below.
  curl::CurlIoLoop::Release();
  CompletionQueue* queue = nullptr;
  {
    MutexLock lock(*g_completion_mutex);
    FIREBASE_ASSERT_MESSAGE(
        g_completion_ref_count > 0,
        "CleanupTransportCurl() without matching InitTransportCurl()");
    if (--g_completion_ref_count == 0) {
      queue = g_completion_queue;
      g_completion_queue = nullptr;
    }
  }
  // Drained outside the lock: a completion callback is allowed to call
  // InitTransportCurl() / CleanupTransportCurl() itself.
  delete queue;
}

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
      read_aborted_(false),
      paused_(false) {
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
  std::function<void()> completion = MakeCompletion();
  if (transport->is_async()) {
    // If this is an asynchronous operation, MarkFailed() or MarkCompleted()
    // could end up attempting to tear down TransportCurl so we signal
    // completion here. Nothing below may touch `transport`.
    transport->SignalTransferComplete();
    // Deliver off the I/O thread (see CompletionQueue). Falls back to inline
    // delivery only when the transport is not initialized at all, i.e. the
    // caller never called InitTransportCurl().
    if (!PostCompletion(completion)) completion();
  } else {
    // Synchronous operations need all data present in the response before
    // Perform() returns so signal complete after MarkFailed() or
    // MarkCompleted(). The caller is blocked in Perform(), so running the
    // completion here (on the I/O thread) cannot run arbitrary user code.
    completion();
    transport->SignalTransferComplete();
  }
}

std::function<void()> RestTransfer::MakeCompletion() const {
  Request* request = request_;
  Response* response = response_;
  if (canceled_) {
    return [request, response]() {
      response->set_status(rest::util::HttpNoContent);
      request->MarkFailed();
      response->MarkFailed();
    };
  }
  if (timed_out_) {
    return [request, response]() {
      response->set_status(rest::util::HttpRequestTimeout);
      request->MarkFailed();
      response->MarkFailed();
    };
  }
  return [request, response]() {
    request->MarkCompleted();
    response->MarkCompleted();
  };
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
  RestTransfer* transfer = static_cast<RestTransfer*>(userdata);
  // libcurl buffers the data and redelivers it after curl_easy_pause(CONT).
  if (transfer->paused_) return CURL_WRITEFUNC_PAUSE;
  // Size is always 1, see https://curl.haxx.se/mail/lib-2010-12/0123.html.
  if (transfer->response_->ProcessHeader(buffer, size * nitems)) {
    return size * nitems;
  } else {
    return 0;
  }
}

size_t RestTransfer::WriteCallback(char* buffer, size_t size, size_t nmemb,
                                   void* userdata) {
  FIREBASE_ASSERT_RETURN(0, userdata != nullptr);
  RestTransfer* transfer = static_cast<RestTransfer*>(userdata);
  if (transfer->paused_) return CURL_WRITEFUNC_PAUSE;
  // Size is always 1, see https://curl.haxx.se/mail/lib-2010-12/0123.html.
  if (transfer->response_->ProcessBody(buffer, size * nmemb)) {
    return size * nmemb;
  } else {
    return 0;
  }
}

size_t RestTransfer::ReadCallback(char* buffer, size_t size, size_t nitems,
                                  void* userdata) {
  FIREBASE_ASSERT_RETURN(0, userdata != nullptr);
  RestTransfer* transfer = static_cast<RestTransfer*>(userdata);
  if (transfer->paused_) return CURL_READFUNC_PAUSE;
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
  CheckOk(curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, err_buf_),
          "set error buffer");

  // Ensure that we are only accepting HTTP(S) traffic.
  // TODO(73540740): Remove support for HTTP.
  CheckOk(curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "http,https"),
          "set valid protocols");

  // libcurl is driven from a dedicated thread: never let it install signal
  // handlers (SIGPIPE save/ignore/restore around every curl_multi_perform(),
  // SIGALRM for name resolution), which race with handlers the application
  // installs on other threads.
  CheckOk(curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L), "disable curl signals");

  // Peer/hostname verification, TLS 1.2 minimum, and OS trust store selection
  // are centralized in app/rest/curl/tls_policy.h.
  CheckOk(curl::ApplyTlsPolicy(easy), "apply TLS policy");

  // Set callback functions.
  CheckOk(curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, &HeaderCallback),
          "set http header callback");
  CheckOk(curl_easy_setopt(easy, CURLOPT_HEADERDATA, static_cast<void*>(this)),
          "set http header callback data");
  CheckOk(curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &WriteCallback),
          "set http body write callback");
  CheckOk(curl_easy_setopt(easy, CURLOPT_WRITEDATA, static_cast<void*>(this)),
          "set http body write callback data");
  CheckOk(curl_easy_setopt(easy, CURLOPT_READFUNCTION, &ReadCallback),
          "set http body read callback");
  CheckOk(curl_easy_setopt(easy, CURLOPT_READDATA, static_cast<void*>(this)),
          "set http body read callback data");
  CheckOk(curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS,
                           static_cast<long>(options.timeout_ms)),  // NOLINT
          "set http timeout milliseconds");

  // Progress reporting for ControllerCurl. libcurl invokes the callback from
  // within curl_multi_perform() on the I/O thread.
  if (control_) {
    CheckOk(curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L), "enable progress");
    CheckOk(curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, &ProgressCallback),
            "set progress callback");
    CheckOk(
        curl_easy_setopt(easy, CURLOPT_XFERINFODATA, static_cast<void*>(this)),
        "set progress callback data");
  }

  std::string method = util::ToUpper(options.method);
  const bool stream_unknown_post_size =
      (method == util::kPost && options.stream_post_fields &&
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
  CheckOk(curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, http_version),
          "set http version");

  // SDK error in initialization stage is not recoverable.
  FIREBASE_ASSERT(err_code_ == CURLE_OK);

  err_code_ = CURLE_OK;

  // Set VERBOSE for debug; does not affect the HTTP connection.
  if (options.verbose) {
    curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
  }

  headers_.reset();
  auto append_header = [this](const char* header) {
    curl_slist* next = curl_slist_append(headers_.get(), header);
    if (next != nullptr) {
      headers_.release();
      headers_.reset(next);
    }
  };
  for (const auto& pair : options.header) {
    std::string header;
    header.reserve(pair.first.size() + pair.second.size() + 1);
    header.append(pair.first);
    header.append(1, util::kHttpHeaderSeparator);
    header.append(pair.second);
    append_header(header.c_str());
  }
  if (method == util::kPost && options.stream_post_fields) {
    size_t transfer_size = request_->GetPostFieldsSize();
    // If the upload size is unknown use chunked encoding.
    if (stream_unknown_post_size) {
      // To use POST to a HTTP 1.1 sever we need to use chunked encoding.
      // This is not supported by HTTP 1.0 servers which need to know the size
      // of the request up front.
      append_header("Transfer-Encoding: chunked");
    } else {
      // Set the expected POST size, to be compatible with HTTP 1.0 servers.
      curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE,
                       static_cast<curl_off_t>(transfer_size));
      if (control_) {
        control_->SetTransferSize(static_cast<int64_t>(transfer_size));
      }
    }
  }

  if (headers_ != nullptr) {
    CheckOk(curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers_.get()),
            "set http header");
  }

  // Set URL
  CheckOk(curl_easy_setopt(easy, CURLOPT_URL, options.url.c_str()),
          "set http url");
  // Set the method.
  if (method == util::kGet) {
    CheckOk(curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L), "set http method");
  } else if (method == util::kPost) {
    CheckOk(curl_easy_setopt(easy, CURLOPT_POST, 1L), "set http method");
  } else {
    // For all other method. Note: If your code is depending on a documented
    // method e.g. PUT, try add another else-if branch above instead of relying
    // on this generic mechanism.
    CheckOk(curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method.c_str()),
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
    CheckOk(
        curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE,
                         static_cast<curl_off_t>(options.post_fields.size())),
        "set http post fields");
    CheckOk(
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, options.post_fields.c_str()),
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
      // A Pause() issued before the I/O thread picked the transfer up: the
      // posted PauseOnIoThread() runs after us, but the data callbacks honor
      // paused_ from the very first byte either way.
      paused_ = control_->paused;
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
  paused_ = true;
  // Takes effect immediately when libcurl already has a connection; otherwise
  // it fails harmlessly and the callbacks pause the transfer at the first
  // byte instead (see paused_).
  if (easy_ != nullptr) curl_easy_pause(easy_.get(), CURLPAUSE_ALL);
}

void RestTransfer::Resume() {
  paused_ = false;
  // Unpauses both an explicit CURLPAUSE_ALL and a callback-initiated pause.
  // Harmless when nothing was paused yet.
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
    : is_async_(false),
      running_transfers_(0),
      running_transfers_semaphore_(0) {}

TransportCurl::~TransportCurl() { WaitForAllTransfersToComplete(); }

void TransportCurl::PerformInternal(
    Request* request, Response* response,
    flatbuffers::unique_ptr<Controller>* controller_out) {
  // Snapshot before anything below can run product code: MarkFailed() /
  // MarkCompleted() are allowed to destroy this TransportCurl (see
  // ~RestTransfer), so `this` must not be read after the transfer is handed
  // off.
  const bool is_async = is_async_;
  std::shared_ptr<TransferControl> control;
  if (controller_out) {
    TransferDirection direction = (request->options().method == util::kGet)
                                      ? kTransferDirectionDownload
                                      : kTransferDirectionUpload;
    control = std::make_shared<TransferControl>(direction);
    // Normally we would use make_new() here, but this is not a std::unique_ptr
    // and make_new() isn't supported by all targets we build for. Published
    // before the transfer starts so Controller calls work even before the I/O
    // thread has picked the transfer up.
    controller_out->reset(new ControllerCurl(control));  // NOLINT
  }
  {
    MutexLock lock(running_transfers_mutex_);
    running_transfers_++;
  }

  if (!is_async && curl::CurlIoLoop::IsCurrentThreadIoThread()) {
    // A synchronous Perform() from the I/O thread itself (e.g. from inside a
    // Response::MarkCompleted() override) can never complete: the thread that
    // would run the transfer is the one that would block here. Fail the
    // request instead of hanging every transfer in the process.
    LogError(
        "Synchronous TransportCurl::Perform() called on the curl I/O thread; "
        "failing the request. Use set_is_async(true) or move the call to "
        "another thread.");
    FIREBASE_DEV_ASSERT_MESSAGE(false,
                                "Synchronous TransportCurl::Perform() on "
                                "the curl I/O thread");
    RestTransfer* transfer = new RestTransfer(this, request, response, control);
    transfer->AbortWithoutLoop();
    return;
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

  // TODO(b/67899466): Remove this once we transition to exclusively
  // asynchronous curl requests.
  if (!is_async) {
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
